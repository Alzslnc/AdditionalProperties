#include "DynamicPropertyNativeBase.h" // Наш изолированный нативный паспорт
#include "Core/PropertyEngine.h"       // Ядро кэширования ОЗУ
#include "ArxBridgeCore.h"             // Заголовок текущего шлюза
#include <vcclr.h>                     // Маршалинг PtrToStringChars
#include "dbapserv.h"                  // Базовый ObjectARX (без OPM конфликтов)
#include "dbmain.h"                    // Описывает класс AcDbDatabase и AcDbEntity


// Подключаем ObjectARX менеджер палитр для регистрации
#include <dynprops.h> 

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    /// <summary>
    /// Нативный COM-транслятор палитры AutoCAD, полностью реализующий IDynamicProperty2.
    /// Наследуется от нативного базового класса и пробивает CLR-границу в C# кэш.
    /// </summary>
    class DynamicPropertyBridge : public DynamicPropertyNativeBase
    {
    private:
        ArxBridge::PropType m_propType;
        IDynamicPropertyNotify2* m_pNotifySink = nullptr; // COM-труба уведомлений палитры

    public:
        DynamicPropertyBridge(System::String^ name, System::String^ description, ArxBridge::PropType type)
            : DynamicPropertyNativeBase() // 🚀 Вызываем базовый пустой конструктор
        {
            // Теперь pin_ptr объявлены как локальные переменные — компилятор счастлив!
            pin_ptr<const wchar_t> wname = PtrToStringChars(name);
            pin_ptr<const wchar_t> wdescription = PtrToStringChars(description);

            // Напрямую заполняем protected-поля нативной базы
            m_nativePropName = wname;
            m_nativeDescription = wdescription;
            m_propType = type;
        }

        virtual ~DynamicPropertyBridge() {}

        // --- Реализация переопределенных CLI-событий ---

        // Событие 1: AutoCAD начинает раунд кадра
        void OnGetDisplayName(BSTR* pBSTRName) override {
            if (!pBSTRName) return;
            *pBSTRName = ::SysAllocString(m_nativePropName.c_str());

            // Сброс и автомата, и векторов типов происходит строго один раз за кадр!
            if (ArxBridgeCore::GetState() != BridgeState::Read) {
                if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                    ArxBridgeCore::m_frameSelectedIds->clear();
                }
                ArxBridgeCore::SetState(BridgeState::Read);
            }
        }

        // Событие 2: Опрос доступности ячейки для редактирования
        void OnIsPropertyReadOnly(BOOL* bIsReadOnly) override {
            if (!bIsReadOnly) return;
            *bIsReadOnly = TRUE;
            if (!m_currentId.isValid()) return;

            AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
            if (pDb && m_currentId.isValid()) {
                bool isReadOnlyFromCache = true;
                if (PropertyEngine::GetPropertyValue(pDb, m_currentId, m_nativePropName, isReadOnlyFromCache)) {
                    *bIsReadOnly = isReadOnlyFromCache ? TRUE : FALSE;
                }
            }
        }

        // Событие 3: Фаза 1. Накопление уникальных типов-маркеров под фильтр OPM
        void OnIsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) override {
            if (!pbEnabled || !pUnk) return;
            *pbEnabled = TRUE;

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {
                if (ArxBridgeCore::GetState() == BridgeState::Read && ArxBridgeCore::m_frameSelectedIds != nullptr) {
                    if (std::find(ArxBridgeCore::m_frameSelectedIds->begin(), ArxBridgeCore::m_frameSelectedIds->end(), nativeId) == ArxBridgeCore::m_frameSelectedIds->end()) {
                        ArxBridgeCore::m_frameSelectedIds->push_back(nativeId);
                    }
                }
            }
        }

        // Событие 4: Жесткое зануление типа (палитра ждет VT_BSTR)
        void OnGetCurrentValueType(VARTYPE* pVarType) override {
            if (!pVarType) return;
            *pVarType = VT_BSTR;
        }
        // Событие 5: AutoCAD подключает свойство к живой палитре
        void OnConnect(IUnknown* pSink) override {
            if (pSink == nullptr) return;
            // Кастим нативный указатель к интерфейсу уведомлений палитры AutoCAD
            m_pNotifySink = reinterpret_cast<IDynamicPropertyNotify2*>(pSink);
            m_pNotifySink->AddRef();

            if (ArxBridgeCore::GetState() == BridgeState::Read) {
                ArxBridgeCore::SetState(BridgeState::Locked);
                if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                    PropertyEngine::PrepareInitialSelectionContext(*ArxBridgeCore::m_frameSelectedIds);
                }
            }
        }

        // Событие 6: Сброс сессии
        void OnDisconnect() override {
            if (m_pNotifySink != nullptr) {
                m_pNotifySink->Release();
                m_pNotifySink = nullptr;
            }
            if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                ArxBridgeCore::m_frameSelectedIds->clear();
            }
            ArxBridgeCore::SetState(BridgeState::Read);
        }

        // Событие 7: Наполнение ячеек палитры строками значений
        void OnGetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) override {
            if (pVarData == nullptr || pUnk == nullptr) return;
            ::VariantInit(pVarData);

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {
                m_currentId = nativeId;
                BridgeState currentState = ArxBridgeCore::GetState();

                // 🏢 ФАЗА ПАКЕТНОЙ ЗАПИСИ (User нажал Enter)
                if (currentState == BridgeState::Write) {
                    ArxBridgeCore::SetState(BridgeState::Locked);
                    PropertyEngine::CommitSelectionChanges(nativeId.database());
                    ArxBridgeCore::SetState(BridgeState::Read);

                    if (m_pNotifySink != nullptr) {
                        m_pNotifySink->OnChanged(reinterpret_cast<IDynamicProperty2*>(this));
                    }
                }

                // 🚀 ФАЗА ЛЕНИВЫХ ТУЛТИПОВ (Connect пропущен, статус Read)
                if (ArxBridgeCore::GetState() == BridgeState::Read) {
                    ArxBridgeCore::SetState(BridgeState::Locked);
                    PropertyEngine::PrepareSelectionContext(nativeId.database(), { nativeId });
                }

                // 📤 ВЫДАЧА ЗНАЧЕНИЯ ИЗ КЭША С#
                std::wstring stringValue;
                if (PropertyEngine::GetPropertyValue(nativeId.database(), nativeId, m_nativePropName, stringValue)) {
                    pVarData->vt = VT_BSTR;
                    pVarData->bstrVal = ::SysAllocString(stringValue.c_str());
                    return;
                }
            }
            pVarData->vt = VT_BSTR;
            pVarData->bstrVal = ::SysAllocString(L"");
        }
        // Событие 8: Пользователь изменил ячейку на палитре
        HRESULT OnSetCurrentValueData(IUnknown* pUnk, const VARIANT varData) override {
            if (varData.vt != VT_BSTR || varData.bstrVal == nullptr || pUnk == nullptr) return E_INVALIDARG;

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {
                std::wstring enteredValue = varData.bstrVal;

                if (PropertyEngine::SetPropertyValue(nativeId.database(), nativeId, m_nativePropName, enteredValue)) {
                    // Только взводим триггер Write. Коммит произойдет пакетом на чтении!
                    ArxBridgeCore::SetState(BridgeState::Write);

                    if (m_pNotifySink != nullptr) {
                        m_pNotifySink->OnChanged(reinterpret_cast<IDynamicProperty2*>(this));
                    }
                    return S_OK;
                }
            }
            return E_FAIL;
        }

        // 🚀 Событие 9: Динамическое распределение ячеек по группам из C# поля .Group!
        std::wstring OnGetCategoryName(AcDbObjectId id) override {
            System::String^ managedGroupName = L"КМ / КЖ Параметры"; // Дефолтное имя группы

            ArxBridge::Property^ prop = PropertyEngine::FindPropertyInCache(id, m_nativePropName);
            if (prop != nullptr && !System::String::IsNullOrEmpty(prop->Group)) {
                managedGroupName = prop->Group; // Подхватываем кастомную C# группу на лету!
            }

            pin_ptr<const wchar_t> pinGroup = PtrToStringChars(managedGroupName);
            return std::wstring(pinGroup);
        }
    };
    // ============================================================================
    // РЕАЛИЗАЦИЯ СТАТИЧЕСКИХ МЕТОДОВ УПРАВЛЕНИЯ ШЛЮЗОМ ARXBRIDGECORE
    // ============================================================================

    void ArxBridgeCore::Initialize() {
        if (m_registeredProperties == nullptr) {
            m_registeredProperties = new std::vector<IUnknown*>();
        }
        if (m_frameSelectedIds == nullptr) {
            m_frameSelectedIds = new std::vector<AcDbObjectId>();
        }
        m_currentState = BridgeState::Read;
    }

    void ArxBridgeCore::Terminate() {
        UnregisterAllProperties();
        if (m_registeredProperties != nullptr) { delete m_registeredProperties; m_registeredProperties = nullptr; }
        if (m_frameSelectedIds != nullptr) { delete m_frameSelectedIds; m_frameSelectedIds = nullptr; }
    }

    void ArxBridgeCore::SetState(BridgeState newState) { m_currentState = newState; }
    BridgeState ArxBridgeCore::GetState() { return m_currentState; }

    void ArxBridgeCore::RegisterDynamicPropertyForClasses(
        System::String^ propertyName,
        System::String^ description,
        PropType type,
        System::Collections::Generic::List<System::String^>^ targetClasses)
    {
        if (System::String::IsNullOrEmpty(propertyName) || targetClasses == nullptr || targetClasses->Count == 0) return;

        // Извлекаем базовый менеджер свойств сущностей чертежа
        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());
        if (pPropMan == nullptr) return;

        for each (System::String ^ managedClassName in targetClasses)
        {
            if (System::String::IsNullOrEmpty(managedClassName)) continue;

            IntPtr ipClass = System::Runtime::InteropServices::Marshal::StringToHGlobalUni(managedClassName);
            const wchar_t* nativeClassName = static_cast<const wchar_t*>(ipClass.ToPointer());

            // Ищем нативный дескриптор класса в словаре ядра AutoCAD по текстовому имени
            AcRxClass* pTargetClass = nullptr;
            if (acrxClassDictionary != nullptr) {
                pTargetClass = AcRxClass::cast(acrxClassDictionary->at(nativeClassName));
            }

            System::Runtime::InteropServices::Marshal::FreeHGlobal(ipClass);

            if (pTargetClass != nullptr)
            {
                // Создаем наш гибридный мост-десант
                DynamicPropertyBridge* pNewProperty = new DynamicPropertyBridge(propertyName, description, type);
                IUnknown* pUnkProp = reinterpret_cast<IUnknown*>(pNewProperty);

                pUnkProp->AddRef();

                // Точечно привязываем свойство СТРОГО к этому классу примитивов чертежа!
                IPropertyManager2* pTargetPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(pTargetClass);
                if (pTargetPropMan != nullptr) {
                    pTargetPropMan->AddProperty(pUnkProp);
                }
                m_registeredProperties->push_back(pUnkProp);
            }
        }
    }

    void ArxBridgeCore::RegisterDynamicProperty(System::String^ propertyName, System::String^ propertyDescription, ArxBridge::PropType propType)
    {
        if (System::String::IsNullOrEmpty(propertyName)) return;

        DynamicPropertyBridge* pNewProperty = new DynamicPropertyBridge(propertyName, propertyDescription, propType);
        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());

        if (pPropMan != nullptr)
        {
            IUnknown* pUnkProp = reinterpret_cast<IUnknown*>(pNewProperty);
            pUnkProp->AddRef();
            pPropMan->AddProperty(pUnkProp);
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
