// 1. Защита интерфейса IServiceProvider (должна быть на самом верху)
#define IServiceProvider WINIServiceProvider

// 2. Логика вашего проекта (использует базовые типы)
#include "Core/PropertyEngine.h"
#include "Model/SelectedObjectData.h"

// 3. Импорт ActiveX (должен идти до некоторых заголовков AutoCAD, чтобы не было конфликтов имен)
#pragma warning(push)
#pragma warning(disable: 4278 4192)

#define STRINGIFY(x) #x
#define MACRO_TO_STRING(x) STRINGIFY(x)
#import MACRO_TO_STRING(OARX_TLB) no_namespace rename("Delete", "AcadDelete") rename("Block", "AcadBlock") rename("IServiceProvider", "AcadIServiceProvider")

#pragma warning(pop)

// 4. Ядро вашего моста и системные заголовочные файлы AutoCAD / CLI
#include "ArxBridgeCore.h"
#include <vcclr.h>
#include <axobjref.h>
#include <dynprops.h>
#include "dbapserv.h"

//#include "axtempl.h"
namespace ArxBridge {

    /// <summary>
    /// Нативный COM-транслятор палитры AutoCAD, полностью реализующий IDynamicProperty2.
    /// </summary>
    class DynamicPropertyBridge : public IDynamicProperty2
    {
    private:
        long m_refCount = 1;
        AcDbObjectId m_currentId;
        std::wstring m_nativePropName;
        std::wstring m_nativeDescription;
        ArxBridge::PropType m_propType;   
        IDynamicPropertyNotify2* m_pNotifySink = nullptr;
    public:
        DynamicPropertyBridge(System::String^ name, System::String^ description, ArxBridge::PropType type) {
            pin_ptr<const wchar_t> wname = PtrToStringChars(name);
            pin_ptr<const wchar_t> wdescription = PtrToStringChars(description);
            m_nativePropName = wname;
            m_nativeDescription = wdescription;
            m_propType = type;       
        }

        virtual ~DynamicPropertyBridge() {}

        // Вспомогательная нативная функция для извлечения AcDbObjectId из IUnknown объекта палитры
        AcDbObjectId GetObjectIdFromIUnknown(IUnknown* pUnk) {
            AcDbObjectId id = AcDbObjectId::kNull;
            if (!pUnk) return id;

            IAcadObject* pAcadObj = nullptr;
            if (SUCCEEDED(pUnk->QueryInterface(__uuidof(IAcadObject), (void**)&pAcadObj))) {
                LONG_PTR rawId = 0;
                if (SUCCEEDED(pAcadObj->get_ObjectID(&rawId)) && rawId != 0) {
                    id.setFromOldId(rawId);
                }
                pAcadObj->Release();
            }
            return id;
        }

        // --- Реализация IUnknown ---
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvRaw) override {
            if (!ppvRaw) return E_POINTER;
            if (riid == IID_IUnknown || riid == __uuidof(IDynamicProperty2)) {
                *ppvRaw = static_cast<IDynamicProperty2*>(this);
                AddRef();
                return S_OK;
            }
            *ppvRaw = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override {
            return InterlockedIncrement(&m_refCount);
        }

        STDMETHODIMP_(ULONG) Release() override {
            long res = InterlockedDecrement(&m_refCount);
            if (res == 0) delete this;
            return res;
        }

        // --- Реализация методов IDynamicProperty2 ---

        STDMETHODIMP GetGUID(GUID* pPropGUID) override {
            if (!pPropGUID) return E_POINTER;
            *pPropGUID = GUID_NULL;
            return S_OK;
        }

        // 1. ТОЧКА СБРОСА КАДРА И ОЧИСТКИ ФИЛЬТРА ТИПОВ
        STDMETHODIMP GetDisplayName(BSTR* pBSTRName) override {
            if (!pBSTRName) return E_POINTER;
            *pBSTRName = ::SysAllocString(m_nativePropName.c_str());

            // 🚀 ИСПРАВЛЕНО: Сброс и векторов, и автомата происходит строго 1 раз за кадр — 
            // в момент перехода из Locked/Write в состояние готовности нового кадра Read!
            if (ArxBridgeCore::GetState() != BridgeState::Read) {

                if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                    ArxBridgeCore::m_frameSelectedIds->clear(); // Очищаем старые типы-маркеры
                }

                ArxBridgeCore::SetState(BridgeState::Read); // Открываем ворота
            }

            return S_OK;
        }

        STDMETHODIMP DynamicPropertyBridge::IsPropertyReadOnly(BOOL* bIsReadOnly) override {
            if (!bIsReadOnly) return E_POINTER;

            *bIsReadOnly = TRUE; // По умолчанию блокируем поле из соображений безопасности
            if (!m_currentId.isValid()) return S_OK; // Мягкий выход при холостом старте

            // Достаем активную базу данных текущего чертежа
            AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();

            // Если база валидна и у текущего экземпляра моста есть сохраненный m_currentId
            if (pDb && m_currentId.isValid()) {
                bool isReadOnlyFromCache = true;

                // Опрашиваем кэш в PropertyEngine (передаем переменную по ссылке)
                if (PropertyEngine::GetPropertyValue(pDb, m_currentId, m_nativePropName, isReadOnlyFromCache)) {
                    // Переводим нативный bool в COM-формат TRUE или FALSE
                    *bIsReadOnly = isReadOnlyFromCache ? TRUE : FALSE;
                }
            }
            return S_OK;
        }


        STDMETHODIMP GetDescription(BSTR* pBSTRDescription) override {
            if (!pBSTRDescription) return E_POINTER;
            *pBSTRDescription = ::SysAllocString(m_nativeDescription.c_str());
            return S_OK;
        }

        // 2. МИКРО-НАКОПИТЕЛЬ АКТУАЛЬНЫХ ТИПОВ ФИЛЬТРА ПАНЕЛИ
        STDMETHODIMP IsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) override {
            if (!pbEnabled || !pUnk) return E_POINTER;
            *pbEnabled = TRUE; // Нативная регистрация по AcRxClass уже отсекла чужие классы сущностей

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {

                // 🎯 НАКОПЛЕНИЕ КЛАССОВ ФИЛЬТРА (Фаза 1 метаданных, статус гарантированно Read):
                if (ArxBridgeCore::GetState() == BridgeState::Read && ArxBridgeCore::m_frameSelectedIds != nullptr) {

                    // Сохраняем по одному первому ID для каждого класса объектов, который пропустил фильтр панели!
                    if (std::find(ArxBridgeCore::m_frameSelectedIds->begin(), ArxBridgeCore::m_frameSelectedIds->end(), nativeId) == ArxBridgeCore::m_frameSelectedIds->end()) {
                        ArxBridgeCore::m_frameSelectedIds->push_back(nativeId);
                    }
                }
            }
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueName(BSTR* pbstrName) override {
            if (!pbstrName) return E_POINTER;
            *pbstrName = nullptr;
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) override {
            if (!pVarType) return E_POINTER;

            // 🚀 ЗАЩИТА: По умолчанию всегда заявляем палитре, что тип — СТРОКА.
            // Это спасет AutoCAD от RPC-вылета при холодном старте менеджера свойств!
            *pVarType = VT_BSTR;

            // Если ID объекта еще не валиден (холодный старт регистрации плагина),
            // мы просто мирно выходим, отдавая VT_BSTR.
            if (!m_currentId.isValid()) {
                return S_OK;
            }

            AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
            if (pDb) {
                PropType managedType = PropType::IsString;

                // Опрашиваем кэш, только если объект реально существует
                if (PropertyEngine::GetPropertyType(pDb, m_currentId, m_nativePropName, managedType)) {
                    if (managedType == PropType::IsDouble) {
                        *pVarType = VT_R8;
                    }
                    else if (managedType == PropType::IsInteger) {
                        *pVarType = VT_I4;
                    }
                    else {
                        *pVarType = VT_BSTR;
                    }
                }
            }
            //работаем через строку
            *pVarType = VT_BSTR;
            return S_OK;
        }

        // 3. ПАКЕТНЫЙ ЗАПУСК ДЛЯ ЖИВОЙ ПАНЕЛИ СВОЙСТВ
        STDMETHODIMP Connect(IDynamicPropertyNotify2* pSink) override {
            if (pSink == nullptr) return E_POINTER;
            m_pNotifySink = pSink;
            m_pNotifySink->AddRef(); // Удерживаем COM-ссылку от случайного удаления из ОЗУ

            // 🚀 СТАРТ ТРАНЗАКЦИИ КАДРА: Сработает строго 1 раз для первого свойства палитры!
            if (ArxBridgeCore::GetState() == BridgeState::Read) {
                ArxBridgeCore::SetState(BridgeState::Locked);

                // Передаем наш накопленный вектор-фильтр типов в оркестратор .NET.
                // Вектор пуст не будет, там уже лежат эталонные ID отфильтрованных на палитре классов!
                if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                    PropertyEngine::PrepareInitialSelectionContext(*ArxBridgeCore::m_frameSelectedIds);
                }
            }
            return S_OK;
        }

        // 5. БЕЗОПАСНЫЙ СБРОС И РАЗОРУЖЕНИЕ СЕССИИ СВЯЗИ
        STDMETHODIMP Disconnect() override {
            if (m_pNotifySink != nullptr) {
                m_pNotifySink->Release(); // Честно возвращаем COM-ссылку AutoCAD
                m_pNotifySink = nullptr;
            }
            if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                ArxBridgeCore::m_frameSelectedIds->clear();
            }
            ArxBridgeCore::SetState(BridgeState::Read);
            return S_OK;
        }

        // 4. ВЫДАЧА СТРОК ЗНАЧЕНИЙ + ЛЕНИВЫЙ СПАСИТЕЛЬ ТУЛТИПОВ + АВТО-ОБНОВЛЕНИЕ UI
        STDMETHODIMP GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) override {
            if (pVarData == nullptr || pUnk == nullptr) return E_POINTER;
            ::VariantInit(pVarData);

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {
                m_currentId = nativeId;
                BridgeState currentState = ArxBridgeCore::GetState();

                // 🏢 ФАЗА ПАКЕТНОЙ ЗАПИСИ (Пользователь изменил ячейку и нажал Enter)
                if (currentState == BridgeState::Write) {
                    ArxBridgeCore::SetState(BridgeState::Locked);

                    // Коммит под DocumentLock и быстрой транзакцией .NET
                    PropertyEngine::CommitSelectionChanges(nativeId.database());
                    ArxBridgeCore::SetState(BridgeState::Read);

                    // 🚀 Мгновенный пинок для интерфейса AutoCAD!
                    // Оповещаем палитру, что параметры изменились, убирая ручной перевыбор объектов
                    if (m_pNotifySink != nullptr) {
                        m_pNotifySink->OnChanged(static_cast<IDynamicProperty2*>(this));
                    }
                }

                // 🚀 ФАЗА ТУЛТИПОВ (Если Connect был полностью пропущен и статус остался Read)
                if (ArxBridgeCore::GetState() == BridgeState::Read) {
                    ArxBridgeCore::SetState(BridgeState::Locked);

                    // Передаем массив из ОДНОГО этого наведенного ID. 
                    // Тултип прочитает одну линию под курсором за наносекунду, не трогая грязный экран.
                    PropertyEngine::PrepareSelectionContext(nativeId.database(), { nativeId });
                }

                // 📤 ВЫДАЧА ГОТОВОГО ЗНАЧЕНИЯ ИЗ КЭША С#
                std::wstring stringValue;
                if (PropertyEngine::GetPropertyValue(nativeId.database(), nativeId, m_nativePropName, stringValue)) {
                    pVarData->vt = VT_BSTR;
                    pVarData->bstrVal = ::SysAllocString(stringValue.c_str());
                                     
                    return S_OK;
                }
            }

            pVarData->vt = VT_BSTR;
            pVarData->bstrVal = ::SysAllocString(L"");
            return S_OK;
        }



        STDMETHODIMP SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) override {
            if (varData.vt != VT_BSTR || varData.bstrVal == nullptr || pUnk == nullptr) return E_INVALIDARG;

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {
                std::wstring enteredValue = varData.bstrVal;

                if (PropertyEngine::SetPropertyValue(nativeId.database(), nativeId, m_nativePropName, enteredValue)) {

                    // 🚀 ТОЧЕЧНО: Только взводим флаг Write. Транзакция выполнится пакетом на следующем кадре!
                    ArxBridgeCore::SetState(BridgeState::Write);

                    if (m_pNotifySink != nullptr) {
                        m_pNotifySink->OnChanged(static_cast<IDynamicProperty2*>(this));
                    }
                    return S_OK;
                }
            }
            return E_FAIL;
        }

    };

    void ArxBridgeCore::Initialize() {

        if (m_registeredProperties == nullptr)
        {
            m_registeredProperties = new std::vector<IUnknown*>();
        }
        if (m_frameSelectedIds == nullptr)
        {
            m_frameSelectedIds = new std::vector<AcDbObjectId>();
        }
        m_currentState = BridgeState::Read; // Изначально выставляем Read       
    }

    void ArxBridgeCore::Terminate() {
        UnregisterAllProperties();
        if (m_registeredProperties != nullptr) {
            delete m_registeredProperties;
            m_registeredProperties = nullptr;
        }
        if (m_frameSelectedIds != nullptr) {
            delete m_frameSelectedIds;
            m_frameSelectedIds = nullptr;
        }
    }
    void ArxBridgeCore::SetState(BridgeState newState) {
        m_currentState = newState;
    }

    BridgeState ArxBridgeCore::GetState() {
        return m_currentState;
    }
    void ArxBridgeCore::RegisterDynamicPropertyForClasses(
        System::String^ propertyName,
        System::String^ description,
        PropType type,
        System::Collections::Generic::List<System::String^>^ targetClasses)
    {
        // Защита от пустых данных
        if (System::String::IsNullOrEmpty(propertyName) || targetClasses == nullptr || targetClasses->Count == 0) return;

        // Получаем доступ к глобальному менеджеру динамических свойств палитры AutoCAD
        // Регистрируем менеджер на базовый класс сущностей AcDbEntity, чтобы он открыл OPM-контекст
        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());
        if (pPropMan == nullptr) return;

        // Перебираем имена классов (например: "AcDbLine", "AcDbPolyline"), прилетевшие из метаданных C#
        for each (System::String ^ managedClassName in targetClasses)
        {
            if (System::String::IsNullOrEmpty(managedClassName)) continue;

            // Переводим управляемую строку .NET в нативный указатель wchar_t*
            IntPtr ipClass = System::Runtime::InteropServices::Marshal::StringToHGlobalUni(managedClassName);
            const wchar_t* nativeClassName = static_cast<const wchar_t*>(ipClass.ToPointer());

            // 🎯 МАГИЯ ObjectARX: Ищем нативный дескриптор класса AutoCAD по его точному текстовому имени!
            // Из строки "AcDbLine" получаем жесткий внутренний указатель на AcDbLine::desc() [2]
            AcRxClass* pTargetClass = nullptr;
            if (acrxClassDictionary != nullptr) {
                pTargetClass = AcRxClass::cast(acrxClassDictionary->at(nativeClassName));
            }

            // Обязательно освобождаем выделенную в куче Windows строку маршалинга
            System::Runtime::InteropServices::Marshal::FreeHGlobal(ipClass);

            // Если такой класс поддерживается текущей версией AutoCAD ядра [2]
            if (pTargetClass != nullptr)
            {
                // Создаем экземпляр нашего COM-переводчика для этого свойства
                DynamicPropertyBridge* pNewProperty = new DynamicPropertyBridge(propertyName, description, type);
                IUnknown* pUnkProp = reinterpret_cast<IUnknown*>(pNewProperty);

                // Увеличиваем счетчик ссылок COM, так как объект регистрируется в AutoCAD
                pUnkProp->AddRef();

                // 🚀 ТОЧЕЧНАЯ РЕГИСТРАЦИЯ: Привязываем свойство СТРОГО к этому конкретному классу примитивов!
                // Теперь AutoCAD сам знает, кому показывать свойство, а кому нет [1]
                IPropertyManager2* pTargetPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(pTargetClass);
                if (pTargetPropMan != nullptr) {
                    pTargetPropMan->AddProperty(pUnkProp);
                }
                // Сохраняем указатель в наш глобальный вектор для последующей корректной выгрузки
                m_registeredProperties->push_back(pUnkProp);
            }
        }
    }
    void ArxBridgeCore::RegisterDynamicProperty(System::String^ propertyName, System::String^ propertyDescription, ArxBridge::PropType propType)
    {
        if (System::String::IsNullOrEmpty(propertyName)) return;

        // 2. Создаем нативный COM-мост
        DynamicPropertyBridge* pNewProperty = new DynamicPropertyBridge(
            propertyName,
            propertyDescription,
            propType           
        );

        // 3. Запрашиваем менеджер свойств для AcDbEntity
        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());

        if (pPropMan != nullptr)
        {
            IUnknown* pUnkProp = reinterpret_cast<IUnknown*>(pNewProperty);
            pUnkProp->AddRef();

            pPropMan->AddProperty(pUnkProp);

            // Складываем в нативный вектор (работаем через указатель)
            m_registeredProperties->push_back(pUnkProp);
        }
        else
        {
            delete pNewProperty;
        }
    }
    void ArxBridgeCore::UnregisterAllProperties()
    {
        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());
        if (pPropMan != nullptr)
        {
            for (IUnknown* pProp : *m_registeredProperties)
            {
                pPropMan->RemoveProperty(pProp);
                pProp->Release();
            }
        }
        m_registeredProperties->clear();
    }
}

