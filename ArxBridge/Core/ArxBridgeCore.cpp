#include <windows.h>
#include <Unknwn.h>

// 1. Корневые заголовки ObjectARX SDK (дают полные типы для AcDbObjectId и AcDbDatabase)
#include "arxHeaders.h" 
#include "dbapserv.h"                  
#include "dbmain.h"                    

// 2. Официальный заголовок палитры свойств
#include <dynprops.h> 

// 3. Заголовки вашего проекта
#include "ArxBridgeCore.h"
#include "DynamicPropertyNativeBase.h"
#include "Core/PropertyEngine.h"       // Ваш C# кэш в ОЗУ
#include <vcclr.h>                     // Для PtrToStringChars

namespace ArxBridge {
      

    class DynamicPropertyBridge : public DynamicPropertyNativeBase
    {
    private:
        // 🚀 ЧИСТО НАИВНЫЕ ПОЛЯ: идеальная скорость, никакого лишнего маршалинга при вызовах к кэшу!
        std::wstring m_nativePropName;
        std::wstring m_nativePropDescription;
        ArxBridge::PropType m_propType;

        ::IUnknown* m_pNotifySink;
        ::AcDbObjectId* m_pCurrentId;

    public:
        // Конструктор по-прежнему принимает управляемые строки на входе шлюза,
        // но сразу же делает ГЛУБОКОЕ копирование в нативную память
        DynamicPropertyBridge(System::String^ name, System::String^ description, ArxBridge::PropType type);
        virtual ~DynamicPropertyBridge();

        void OnGetDisplayName(BSTR* pBSTRName) override;
        void OnGetDescription(BSTR* pBSTRDescription) override;
        void OnIsPropertyReadOnly(BOOL* bIsReadOnly) override;
        void OnIsPropertyEnabled(::IUnknown* pUnk, BOOL* pbEnabled) override;
        void OnGetCurrentValueType(VARTYPE* pVarType) override;
        void OnConnect(::IUnknown* m_pNotifySink) override;
        void OnDisconnect() override;
        void OnGetCurrentValueData(::IUnknown* pUnk, ::VARIANT* pVarData) override;
        HRESULT OnSetCurrentValueData(::IUnknown* pUnk, const ::VARIANT varData) override;
        void OnGetCategoryName(BSTR* pbstrName) override;
    };

    DynamicPropertyBridge::DynamicPropertyBridge(System::String^ name, System::String^ description, ArxBridge::PropType type)
        : DynamicPropertyNativeBase()
    {
        // Выполняем глубокое нативное копирование. Теперь данные живут в нативной куче C++
        // и никак не зависят от перемещений сборщика мусора .NET!
        pin_ptr<const wchar_t> wname = PtrToStringChars(name);
        m_nativePropName = wname;

        pin_ptr<const wchar_t> wdesc = PtrToStringChars(description);
        m_nativePropDescription = wdesc;

        m_propType = type;
        m_pNotifySink = nullptr;

        m_pCurrentId = new ::AcDbObjectId();
        *m_pCurrentId = ::AcDbObjectId::kNull;
    }

    DynamicPropertyBridge::~DynamicPropertyBridge()
    {
        if (m_pNotifySink != nullptr) {
            m_pNotifySink->Release();
            m_pNotifySink = nullptr;
        }
        if (m_pCurrentId != nullptr) {
            delete m_pCurrentId;
            m_pCurrentId = nullptr;
        }
    }

    void DynamicPropertyBridge::OnGetDisplayName(BSTR* pBSTRName) {
        if (!pBSTRName) return;
        *pBSTRName = ::SysAllocString(m_nativePropName.c_str());

        if (ArxBridgeCore::GetState() != BridgeState::Read) {
            if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                ArxBridgeCore::m_frameSelectedIds->clear();
            }
            ArxBridgeCore::SetState(BridgeState::Read);
        }
    }

    void DynamicPropertyBridge::OnGetDescription(BSTR* pBSTRDescription) {
        if (!pBSTRDescription) return;
        *pBSTRDescription = ::SysAllocString(m_nativePropDescription.c_str());
    }

    void DynamicPropertyBridge::OnIsPropertyReadOnly(BOOL* bIsReadOnly) {
        if (!bIsReadOnly) return;
        *bIsReadOnly = TRUE;
        if (!m_pCurrentId->isValid()) return;

        AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
        if (pDb && m_pCurrentId->isValid()) {
            bool isReadOnlyFromCache = true;
            // 🚀 ИДЕАЛЬНО: Передаем нативную строку напрямую!
            if (PropertyEngine::GetPropertyValue(pDb, *m_pCurrentId, m_nativePropName, isReadOnlyFromCache)) {
                *bIsReadOnly = isReadOnlyFromCache ? TRUE : FALSE;
            }
        }
    }

    void DynamicPropertyBridge::OnIsPropertyEnabled(::IUnknown* pUnk, BOOL* pbEnabled) {
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

    void DynamicPropertyBridge::OnGetCurrentValueType(VARTYPE* pVarType) {
        if (!pVarType) return;
        *pVarType = VT_BSTR;
    }

    void DynamicPropertyBridge::OnConnect(::IUnknown* pSink) {
        if (pSink == nullptr) return;
        m_pNotifySink = pSink;
        m_pNotifySink->AddRef();

        if (ArxBridgeCore::GetState() == BridgeState::Read) {
            ArxBridgeCore::SetState(BridgeState::Locked);
            if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
                PropertyEngine::PrepareInitialSelectionContext(*ArxBridgeCore::m_frameSelectedIds);
            }
        }
    }

    void DynamicPropertyBridge::OnDisconnect() {
        if (m_pNotifySink != nullptr) {
            m_pNotifySink->Release();
            m_pNotifySink = nullptr;
        }
        if (ArxBridgeCore::m_frameSelectedIds != nullptr) {
            ArxBridgeCore::m_frameSelectedIds->clear();
        }
        ArxBridgeCore::SetState(BridgeState::Read);
        *m_pCurrentId = AcDbObjectId::kNull;
    }

    void DynamicPropertyBridge::OnGetCurrentValueData(::IUnknown* pUnk, ::VARIANT* pVarData) {
        if (pVarData == nullptr || pUnk == nullptr) return;
        ::VariantInit(pVarData);

        AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
        if (nativeId.isValid() && nativeId.database() != nullptr) {
            *m_pCurrentId = nativeId;
            BridgeState currentState = ArxBridgeCore::GetState();

            if (currentState == BridgeState::Write) {
                ArxBridgeCore::SetState(BridgeState::Locked);
                PropertyEngine::CommitSelectionChanges(nativeId.database());
                ArxBridgeCore::SetState(BridgeState::Read);

                if (m_pNotifySink != nullptr) {
                    if (m_pNotifySink != nullptr) {
                        // Делаем аккуратный QueryInterface по текстовому UUID интерфейса уведомлений палитры
                        // IID_IDynamicPropertyNotify2 равен {0x5F228E11-53D2-473d-9C21-BC20D00C4465}
                        static const GUID IID_IDynamicPropertyNotify2 = { 0x5F228E11, 0x53D2, 0x473d, { 0x9C, 0x21, 0xBC, 0x20, 0xD0, 0x0C, 0x44, 0x65 } };

                        void* pNotify2 = nullptr;
                        if (SUCCEEDED(m_pNotifySink->QueryInterface(IID_IDynamicPropertyNotify2, &pNotify2))) {
                            // Вызываем метод OnChanged по его жесткому бинарному смещению в vtable (это первый метод интерфейса)
                            // Сигнатура метода: void OnChanged(IUnknown* pDynamicProperty)
                            typedef void(__stdcall* OnChangedFunc)(void*, void*);
                            OnChangedFunc* vtable = *(OnChangedFunc**)pNotify2;
                            vtable[0](pNotify2, this); // Передаем себя в качестве свойства

                            ((::IUnknown*)pNotify2)->Release();
                        }
                    }
                }
            }

            if (ArxBridgeCore::GetState() == BridgeState::Read) {
                ArxBridgeCore::SetState(BridgeState::Locked);
                PropertyEngine::PrepareSelectionContext(nativeId.database(), { nativeId });
            }

            std::wstring stringValue;
            // 🚀 ИДЕАЛЬНО: Никакого оверхеда на маршалинг кадра
            if (PropertyEngine::GetPropertyValue(nativeId.database(), nativeId, m_nativePropName, stringValue)) {
                pVarData->vt = VT_BSTR;
                pVarData->bstrVal = ::SysAllocString(stringValue.c_str());
                return;
            }
        }
        pVarData->vt = VT_BSTR;
        pVarData->bstrVal = ::SysAllocString(L"");
    }

    HRESULT DynamicPropertyBridge::OnSetCurrentValueData(::IUnknown* pUnk, const ::VARIANT varData) {
        if (varData.vt != VT_BSTR || varData.bstrVal == nullptr || pUnk == nullptr) return E_INVALIDARG;

        AcDbObjectId nativeId = GetObjectIdFromIUnknown(pUnk);
        if (nativeId.isValid() && nativeId.database() != nullptr) {
            std::wstring enteredValue = varData.bstrVal;

            // 🚀 ИДЕАЛЬНО: Передаем имя и значение без .NET прослоек
            if (PropertyEngine::SetPropertyValue(nativeId.database(), nativeId, m_nativePropName, enteredValue)) {
                ArxBridgeCore::SetState(BridgeState::Write);

                if (m_pNotifySink != nullptr) {
                    if (m_pNotifySink != nullptr) {
                        // Делаем аккуратный QueryInterface по текстовому UUID интерфейса уведомлений палитры
                        // IID_IDynamicPropertyNotify2 равен {0x5F228E11-53D2-473d-9C21-BC20D00C4465}
                        static const GUID IID_IDynamicPropertyNotify2 = { 0x5F228E11, 0x53D2, 0x473d, { 0x9C, 0x21, 0xBC, 0x20, 0xD0, 0x0C, 0x44, 0x65 } };

                        void* pNotify2 = nullptr;
                        if (SUCCEEDED(m_pNotifySink->QueryInterface(IID_IDynamicPropertyNotify2, &pNotify2))) {
                            // Вызываем метод OnChanged по его жесткому бинарному смещению в vtable (это первый метод интерфейса)
                            // Сигнатура метода: void OnChanged(IUnknown* pDynamicProperty)
                            typedef void(__stdcall* OnChangedFunc)(void*, void*);
                            OnChangedFunc* vtable = *(OnChangedFunc**)pNotify2;
                            vtable[0](pNotify2, this); // Передаем себя в качестве свойства

                            ((::IUnknown*)pNotify2)->Release();
                        }
                    }
                }
                return S_OK;
            }
        }
        return E_FAIL;
    }

    void DynamicPropertyBridge::OnGetCategoryName(BSTR* pbstrName) {
        if (!pbstrName) return;
        System::String^ managedGroupName = L"КМ / КЖ Параметры";

        if (m_pCurrentId->isValid()) {
            // 🚀 ИДЕАЛЬНО: Нативный поиск в кэше
            ArxBridge::Property^ prop = PropertyEngine::FindPropertyInCache(*m_pCurrentId, m_nativePropName);
            if (prop != nullptr && !System::String::IsNullOrEmpty(prop->Group)) {
                managedGroupName = prop->Group;
            }
        }

        pin_ptr<const wchar_t> pinGroup = PtrToStringChars(managedGroupName);
        *pbstrName = ::SysAllocString(pinGroup);
    }
}



namespace ArxBridge {
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
