#include "ArxBridgeCore.h"
#include "DynamicPropertyNativeBase.h"
#include "OpmPropertyService.h"
#include "Core/PropertyEngine.h"       
#include "Core/BridgeManager.h"  
#include <msclr/marshal_cppstd.h>
#include <vcclr.h>
#include <algorithm>
#include "arxHeaders.h" 
#include "dbapserv.h"                  
#include "dbmain.h"  


using namespace System;

namespace ArxBridge {

    // ============================================================================
    // 📝 БЛОК 0. ДЕКЛАРАЦИЯ КЛАССА НАБЛЮДАТЕЛЯ COM
    // ============================================================================
    class DynamicPropertyBridge : public DynamicPropertyNativeBase
    {
    private:
        std::wstring m_nativePropName;
        std::wstring m_nativePropDescription;
        ArxBridge::PropType m_propType;

        ::IUnknown* m_pNotifySink;
        ::AcDbObjectId* m_pCurrentId;

    public:
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
        void OnMapPropertyToCategory(LONG* pPropCat) override;
    };
    // ============================================================================
// 🛠️ БЛОК 1. УПРАВЛЕНИЕ РЕСУРСАМИ И ЖИЗНЕННЫМ ЦИКЛОМ (ФАБРИКА)
// ============================================================================

    DynamicPropertyBridge::DynamicPropertyBridge(System::String^ name, System::String^ description, ArxBridge::PropType type)
        : DynamicPropertyNativeBase()
    {
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

    /// <summary>
    /// Нативная фабрика для создания экземпляра свойства из ArxBridgeCore.cpp
    /// </summary>
    ::IUnknown* CreateDynamicProperty(System::String^ name, System::String^ description, ArxBridge::PropType type) {
        DynamicPropertyBridge* pNewProp = new DynamicPropertyBridge(name, description, type);
        return reinterpret_cast<::IUnknown*>(pNewProp);
    }
    // ============================================================================
// 🔍 БЛОК 2. МЕТАДАННЫЕ СВОЙСТВА И ЖИЗНЕННЫЙ ЦИКЛ КАДРА OPM
// ============================================================================

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

            // 🚀 ИСПРАВЛЕНО: Вызываем специализированный метод проверки флага защиты вместо парсинга значений ячейки!
            if (OpmPropertyService::IsPropertyReadOnly(pDb, *m_pCurrentId, m_nativePropName, isReadOnlyFromCache)) {
                *bIsReadOnly = isReadOnlyFromCache ? TRUE : FALSE;
            }
        }
    }

    void DynamicPropertyBridge::OnIsPropertyEnabled(::IUnknown* pUnk, BOOL* pbEnabled) {
        if (!pbEnabled || !pUnk) return;
        *pbEnabled = TRUE;

        AcDbObjectId nativeId = ::AcDbObjectId::kNull;
        GetObjectIdFromIUnknown(pUnk, &nativeId);
        if (nativeId.isValid() && nativeId.database() != nullptr) {
            if (m_pCurrentId != nullptr) {
                *m_pCurrentId = nativeId;
            }

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
    // ============================================================================
    // 🔄 БЛОК 3. КОНВЕЙЕР ОБМЕНА ДАННЫМИ (ФАЗЫ GET / SET) С ПАНЕЛЬЮ AUTOCAD
    // ============================================================================

    void DynamicPropertyBridge::OnGetCurrentValueData(::IUnknown* pUnk, ::VARIANT* pVarData) {
        if (pVarData == nullptr || pUnk == nullptr) return;
        ::VariantInit(pVarData);

        AcDbObjectId nativeId = ::AcDbObjectId::kNull;
        GetObjectIdFromIUnknown(pUnk, &nativeId);

        if (nativeId.isValid() && nativeId.database() != nullptr) {
            *m_pCurrentId = nativeId;
            BridgeState currentState = ArxBridgeCore::GetState();

            // 📥 ТАКТ 1. ФИКСАЦИЯ ИЗМЕНЕНИЙ (Если пользователь только что нажал Enter/сменил фокус)
            if (currentState == BridgeState::Write) {                
                PropertyEngine::CommitSelectionChanges(nativeId.database());
                ArxBridgeCore::SetState(BridgeState::Locked);

                if (m_pNotifySink != nullptr) {
                    static const GUID IID_IDynamicPropertyNotify2 = { 0x5F228E11, 0x53D2, 0x473d, { 0x9C, 0x21, 0xBC, 0x20, 0xD0, 0x0C, 0x44, 0x65 } };
                    void* pNotify2 = nullptr;
                    if (SUCCEEDED(m_pNotifySink->QueryInterface(IID_IDynamicPropertyNotify2, &pNotify2))) {
                        typedef void(__stdcall* OnChangedFunc)(void*, void*);
                        OnChangedFunc* vtable = *(OnChangedFunc**)pNotify2;
                        vtable[0](pNotify2, this); // Уведомляем палитру об успешном обновлении
                        ((::IUnknown*)pNotify2)->Release();
                    }
                }
            }

            // 📥 ТАКТ 2. УМНАЯ ЛЕНИВАЯ ИНИЦИАЛИЗАЦИЯ (Срабатывает только для точечного Тултипа)
            // Если OnConnect не вызывался, автомат остался в состоянии Read.
            // Точечно обновляем кэш строго для одного этого объекта под курсором.
            if (ArxBridgeCore::GetState() == BridgeState::Read) {
                ArxBridgeCore::SetState(BridgeState::Locked);
                PropertyEngine::PrepareSelectionContext(nativeId.database(), { nativeId });
            }

            // 📥 ТАКТ 3. ВЫДАЧА ДАННЫХ ИЗ ГОТОВОГО КЭША (Максимальная скорость O(1))
            std::wstring stringValue;
            // 🚀 ИСПРАВЛЕНО: Вызываем новый оптимизированный OpmPropertyService вместо старого PropertyEngine
            if (OpmPropertyService::GetPropertyValue(nativeId.database(), nativeId, m_nativePropName, stringValue)) {
                pVarData->vt = VT_BSTR;
                pVarData->bstrVal = ::SysAllocString(stringValue.c_str());
                return;
            }
        }

        // Дефолтный ответ, если свойства нет в кэше плагина
        pVarData->vt = VT_BSTR;
        pVarData->bstrVal = ::SysAllocString(L"");
    }

    HRESULT DynamicPropertyBridge::OnSetCurrentValueData(::IUnknown* pUnk, const ::VARIANT varData) {
        if (pUnk == nullptr) return E_INVALIDARG;

        // AutoCAD OPM передает текстовые данные как VT_BSTR
        if (varData.vt != VT_BSTR) return E_INVALIDARG;

        AcDbObjectId nativeId = ::AcDbObjectId::kNull;
        GetObjectIdFromIUnknown(pUnk, &nativeId);

        if (nativeId.isValid() && nativeId.database() != nullptr) {
            // 🚀 ЗАЩИТА ОТ КРАША (Access Violation): 
            // Если пользователь полностью стёр текст в ячейке, bstrVal придет как nullptr.
            // Заменяем его на пустую нативную строку L"", предотвращая падение конструктора std::wstring.
            std::wstring enteredValue = (varData.bstrVal != nullptr) ? varData.bstrVal : L"";

            // 🚀 ИСПРАВЛЕНО: Запись транслируется через новый OpmPropertyService
            if (OpmPropertyService::SetPropertyValue(nativeId.database(), nativeId, m_nativePropName, enteredValue)) {

                // Взводим автомат в состояние Write. Следующий же такт OnGetCurrentValueData 
                // заблокирует документ и вызовет пакетный коммит изменений в базу данных!
                ArxBridgeCore::SetState(BridgeState::Write);

                if (m_pNotifySink != nullptr) {
                    static const GUID IID_IDynamicPropertyNotify2 = { 0x5F228E11, 0x53D2, 0x473d, { 0x9C, 0x21, 0xBC, 0x20, 0xD0, 0x0C, 0x44, 0x65 } };
                    void* pNotify2 = nullptr;
                    if (SUCCEEDED(m_pNotifySink->QueryInterface(IID_IDynamicPropertyNotify2, &pNotify2))) {
                        typedef void(__stdcall* OnChangedFunc)(void*, void*);
                        OnChangedFunc* vtable = *(OnChangedFunc**)pNotify2;
                        vtable[0](pNotify2, this); // Сигнализируем палитре, что данные изменились
                        ((::IUnknown*)pNotify2)->Release();
                    }
                }
                return S_OK;
            }
        }
        return E_FAIL;
    }

    // ============================================================================
    // 🗂️ БЛОК 4. УПРАВЛЕНИЕ КАТЕГОРИЯМИ И ГРУППИРОВКОЙ OPM СВОЙСТВ
    // ============================================================================

    void DynamicPropertyBridge::OnGetCategoryName(BSTR* pbstrName) {
        if (!pbstrName) return;
        System::String^ managedGroupName = gcnew System::String(u8"Дополнительные параметры");
        System::String^ propName = gcnew System::String(m_nativePropName.c_str());

        PropertyMeta^ meta = BridgeManager::FindMetadataAcrossProviders(propName);
        if (meta != nullptr && meta->GroupType == PropertyGroup::Custom && !System::String::IsNullOrEmpty(meta->CustomGroupName)) {
            managedGroupName = meta->CustomGroupName;
        }

        pin_ptr<const wchar_t> pinGroup = PtrToStringChars(managedGroupName);
        *pbstrName = ::SysAllocString(pinGroup);
    }

    void DynamicPropertyBridge::OnMapPropertyToCategory(LONG* pPropCat) {
        if (!pPropCat) return;
        *pPropCat = -3;

        System::String^ propName = gcnew System::String(m_nativePropName.c_str());
        PropertyMeta^ meta = BridgeManager::FindMetadataAcrossProviders(propName);

        if (meta != nullptr) {
            if (meta->GroupType == PropertyGroup::Custom) {
                if (!System::String::IsNullOrEmpty(meta->CustomGroupName)) {
                    LONG hash = meta->CustomGroupName->GetHashCode();
                    *pPropCat = std::abs(hash);
                    if (*pPropCat <= 0) *pPropCat = 1001;
                }
            }
            else {
                *pPropCat = static_cast<LONG>(meta->GroupType);
            }
        }
    }

} // namespace ArxBridge
