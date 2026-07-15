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

    public:
        DynamicPropertyBridge(System::String^ name, System::String^ description, ArxBridge::PropType type) {
            pin_ptr<const wchar_t> wname = PtrToStringChars(name);
            pin_ptr<const wchar_t> wdescription = PtrToStringChars(description);
            m_nativePropName = wname;
            m_nativeDescription = wdescription;
            m_propType = type;       
        }

        virtual ~DynamicPropertyBridge() {}

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
            if (!pbEnabled) return E_POINTER;
            *pbEnabled = TRUE;
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueName(BSTR* pbstrName) override {
            if (!pbstrName) return E_POINTER;
            *pbstrName = nullptr;
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) override {
            if (!pVarType) return E_POINTER;
            *pVarType = (m_propType == ArxBridge::PropType::IsDouble) ? VT_R8 : VT_I4;
            return S_OK;
        }

        STDMETHODIMP Connect(IDynamicPropertyNotify2* pSink) override {
            if (pSink == nullptr) return E_POINTER;

            IAcadObject* pAcadObj = nullptr;
            if (SUCCEEDED(pSink->QueryInterface(__uuidof(IAcadObject), (void**)&pAcadObj)) && pAcadObj != nullptr)
            {
                LONG_PTR rawId = 0;
                if (SUCCEEDED(pAcadObj->get_ObjectID(&rawId)) && rawId != 0)
                {
                    AcDbObjectId nativeId;
                    nativeId.setFromOldId(rawId);

                    if (nativeId.isValid() && nativeId.database() != nullptr)
                    {
                        // 🚀 ШАГ 1: СНАЧАЛА ВСЕГДА ЗАПОМИНАЕМ ID ДЛЯ ТЕКУЩЕГО ЭКЗЕМПЛЯРА СВОЙСТВА!
                        m_currentId = nativeId;

                        // ШАГ 2: ТОЛЬКО ТЕПЕРЬ ОПРАШИВАЕМ НАШ ОБЩИЙ СТАТИЧЕСКИЙ АВТОМАТ
                        BridgeState currentState = ArxBridgeCore::GetState();

                        // Блокировка кадра: если мы уже заблокированы в Connect — просто выходим.
                        // При этом m_currentId у этого экземпляра свойства уже успешно сохранен!
                        if (currentState == BridgeState::Connect) {
                            pAcadObj->Release();
                            return S_OK;
                        }

                        // Пакетная фиксация изменений: если вернулись из SetValue
                        if (currentState == BridgeState::Write) {
                            PropertyEngine::CommitSelectionChanges(nativeId.database());
                        }

                        // Пакетное ленивое чтение геометрии чертежа в C# кэш
                        if (currentState == BridgeState::Read) {
                            PropertyEngine::PrepareSelectionContext(nativeId.database(), { nativeId });
                        }

                        // Универсальный финал: закрываем замок кадра
                        ArxBridgeCore::SetState(BridgeState::Connect);
                    }
                }
                pAcadObj->Release();
            }
            return S_OK;
        }



        STDMETHODIMP Disconnect() override {
            m_currentId = AcDbObjectId::kNull;
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) override {
            if (pVarData == nullptr) return E_POINTER;
            ::VariantInit(pVarData);

            // 1. Возвращаем стабильное извлечение рабочей базы данных из старого метода
            AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
            if (!pDb || !m_currentId.isValid()) return E_FAIL;

            std::wstring stringValue;

            // 2. Вызываем строковый геттер, используя сохраненный m_currentId
            if (PropertyEngine::GetPropertyValue(pDb, m_currentId, m_nativePropName, stringValue)) {
                pVarData->vt = VT_BSTR;
                pVarData->bstrVal = ::SysAllocString(stringValue.c_str());

                // Сброс триггера автомата для следующего кадра палитры
                ArxBridgeCore::SetState(BridgeState::Read);
                return S_OK;
            }

            // 3. Защита: если кэш пуст, отдаем пустую COM-строку, чтобы палитра не зависала
            pVarData->vt = VT_BSTR;
            pVarData->bstrVal = ::SysAllocString(L"");
            ArxBridgeCore::SetState(BridgeState::Read);
            return S_OK;
        }


        STDMETHODIMP SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) override {
            return S_OK;
        }
    };

    void ArxBridgeCore::Initialize() {
        if (m_registeredProperties == nullptr) {
            m_registeredProperties = new std::vector<IUnknown*>();
        }
        m_currentState = BridgeState::Read; // Изначально выставляем Read
    }

    void ArxBridgeCore::Terminate() {
        UnregisterAllProperties();
        if (m_registeredProperties != nullptr) {
            delete m_registeredProperties;
            m_registeredProperties = nullptr;
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

