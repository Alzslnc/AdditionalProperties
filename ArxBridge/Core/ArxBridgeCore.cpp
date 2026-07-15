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

        STDMETHODIMP GetDisplayName(BSTR* pBSTRName) override {
            if (!pBSTRName) return E_POINTER;
            *pBSTRName = ::SysAllocString(m_nativePropName.c_str());
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
            *pBSTRDescription = ::SysAllocString(L"Custom Property");
            return S_OK;
        }

        STDMETHODIMP IsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) override {
            if (!pbEnabled || !pUnk) return E_POINTER;

            // 🚀 РЕШЕНИЕ: Всегда разрешаем, давая палитре скормить нам pUnk объектов.
            // Реальную фильтрацию ячеек сделает C# на этапе чтения данных!
            *pbEnabled = TRUE;

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {

                // Накапливаем точный список ID только во время фазы ожидания чтения
                if (ArxBridgeCore::GetState() == BridgeState::Read) {
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
            return S_OK;
        }

        STDMETHODIMP Connect(IDynamicPropertyNotify2* pSink) override {
            if (pSink == nullptr) return E_POINTER;
            m_pNotifySink = pSink; // Сохраняем "трубу" уведомлений
            return S_OK;
        }



        STDMETHODIMP Disconnect() override {
            m_pNotifySink = nullptr;
            // При полной расфокусировке палитры возвращаем автомат в исходное чтение
            ArxBridgeCore::SetState(BridgeState::Read);
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) override {
            if (pVarData == nullptr || pUnk == nullptr) return E_POINTER;
            ::VariantInit(pVarData);

            AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
            if (nativeId.isValid() && nativeId.database() != nullptr) {
                m_currentId = nativeId;
                BridgeState currentState = ArxBridgeCore::GetState();

                // 1. ФАЗА ЗАПИСИ: применили изменения и сбросили автомат в Read
                if (currentState == BridgeState::Write) {
                    ArxBridgeCore::SetState(BridgeState::Connect);
                    PropertyEngine::CommitSelectionChanges(nativeId.database());
                    ArxBridgeCore::SetState(BridgeState::Read);
                    ArxBridgeCore::m_frameSelectedIds->clear(); // Очищаем для нового кадра
                }

                // 2. ФАЗА ЧТЕНИЯ: сработает строго ОДИН РАЗ на первом объекте в кадре!
                if (ArxBridgeCore::GetState() == BridgeState::Read) {

                    // 🚀 ТОЧЕЧНО: Отдаем в C# уже полностью готовый, собранный в IsEnabled массив ID
                    if (ArxBridgeCore::m_frameSelectedIds != nullptr && !ArxBridgeCore::m_frameSelectedIds->empty()) {
                        PropertyEngine::PrepareSelectionContext(nativeId.database(), *ArxBridgeCore::m_frameSelectedIds);
                    }
                    else {
                        PropertyEngine::PrepareSelectionContext(nativeId.database(), { nativeId });
                    }

                    // Жестко запираем замок кадра. Остальные свойства и объекты просто читают готовый кэш
                    ArxBridgeCore::SetState(BridgeState::Connect);
                }

                // Извлекаем строку из пакетного кэша
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

        m_registeredProperties = new std::vector<IUnknown*>();
        m_frameSelectedIds = new std::vector<AcDbObjectId>();
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

