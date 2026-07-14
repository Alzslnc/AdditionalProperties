// Разведение интерфейсов IServiceProvider на уровне препроцессора [INDEX]
#define IServiceProvider WINIServiceProvider
#define IServiceProvider_INTERFACE_DEFINED_
#define _SERVENT_H_

#include "Core/PropertyEngine.h"
#include "Core/PropertyReactor.h"
#include "Model/SelectedObjectData.h"

// Изолированный блок импорта ActiveX автоматизации AutoCAD
#pragma warning(push)
#pragma warning(disable:4278)
#pragma warning(disable:4192)
#define STRINGIFY(x) #x
#define MACRO_TO_STRING(x) STRINGIFY(x)

#if !defined(OARX_2021)
#import MACRO_TO_STRING(OARX_TLB) no_namespace rename("Delete", "AcadDelete") rename("Block", "AcadBlock") rename("IServiceProvider", "AcadIServiceProvider")
#else
#import "D:/OARX_SDK/V2021/inc-x64/axdb24enu.tlb" no_namespace rename("Delete", "AcadDelete") rename("Block", "AcadBlock") rename("IServiceProvider", "AcadIServiceProvider")
#endif

#pragma warning(pop)

#pragma comment(lib, "acpal.lib")
#include <vcclr.h>
#include <axobjref.h>
#include <dynprops.h>
#include <opmext.h>
#include "dbapserv.h"

namespace ArxBridge {

    // Нативный COM-транслятор палитры AutoCAD со скриншота [INDEX]
    class DynamicPropertyBridge : public IDynamicProperty2
    {
    private:
        long m_refCount = 1;
        AcDbObjectId m_currentId;

        // ИСПРАВЛЕНО: Убрали gcroot подчистую! Заменили на нативную строку std::wstring
        std::wstring m_nativePropName;
        ArxBridge::PropType m_propType;
        bool m_isReadOnly;

    public:
        DynamicPropertyBridge(System::String^ name, ArxBridge::PropType type, bool isReadOnly) {
            pin_ptr<const wchar_t> wname = PtrToStringChars(name);
            m_nativePropName = wname; // Копируем данные в нативную строку std::wstring
            m_propType = type;
            m_isReadOnly = isReadOnly;
        }
        virtual ~DynamicPropertyBridge() {}

        // --- Реализация IUnknown ---
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvRaw) {
            if (!ppvRaw) return E_POINTER;
            if (riid == IID_IUnknown || riid == __uuidof(IDynamicProperty2)) {
                *ppvRaw = static_cast<IDynamicProperty2*>(this);
                AddRef();
                return S_OK;
            }
            *ppvRaw = nullptr;
            return E_NOINTERFACE;
        }
        STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_refCount); }
        STDMETHODIMP_(ULONG) Release() {
            long res = InterlockedDecrement(&m_refCount);
            if (res == 0) delete this;
            return res;
        }

        // --- Реализация методов IDynamicProperty2 ---
        STDMETHODIMP GetGUID(GUID* pPropGUID) { if (!pPropGUID) return E_POINTER; *pPropGUID = GUID_NULL; return S_OK; }

        STDMETHODIMP GetDisplayName(BSTR* pBSTRName) {
            if (!pBSTRName) return E_POINTER;
            // Отдаем нативную строку в COM-буфер палитры напрямую
            *pBSTRName = ::SysAllocString(m_nativePropName.c_str());
            return S_OK;
        }

        STDMETHODIMP IsReadOnly(BOOL* bIsReadOnly) {
            if (!bIsReadOnly) return E_POINTER;
            *bIsReadOnly = m_isReadOnly ? TRUE : FALSE;
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueDataType(VARTYPE* pVarType) {
            if (!pVarType) return E_POINTER;
            *pVarType = (m_propType == ArxBridge::PropType::IsDouble) ? VT_R8 : VT_I4;
            return S_OK;
        }

        STDMETHODIMP GetDescription(BSTR* pBSTRDescription) { if (!pBSTRDescription) return E_POINTER; *pBSTRDescription = ::SysAllocString(L"Custom Property"); return S_OK; }

        // ПЕРЕХВАТ КЛИКА МЫШИ: Вытягиваем нативный AcDbObjectId
          // ИСПРАВЛЕНО: Разделяем сигнатуру под требования старого и нового SDK
        #if defined(OARX_2021)
        STDMETHODIMP Connect(IUnknown** pUnk)
        #else
        STDMETHODIMP Connect(IUnknown* pUnk)
        #endif
        {
            // Для 2021 года извлекаем одиночный указатель из прилетевшего двойного указателя
            #if defined(OARX_2021)
            if (pUnk == nullptr || *pUnk == nullptr) return E_POINTER;
            IUnknown* pRealUnk = *pUnk;
            #else
            if (pUnk == nullptr) return E_POINTER;
            IUnknown* pRealUnk = pUnk;
            #endif

            // Извлекаем нативный ID через базовый COM-интерфейс объекта AutoCAD
            IAcadObject* pAcadObj = nullptr;
            if (SUCCEEDED(pRealUnk->QueryInterface(__uuidof(IAcadObject), (void**)&pAcadObj)) && pAcadObj != nullptr)
            {
                LONG_PTR rawId = 0;
                if (SUCCEEDED(pAcadObj->get_ObjectID(&rawId)) && rawId != 0)
                {
                    AcDbObjectId nativeId;
                    nativeId.setFromOldId(rawId);

                    if (nativeId.isValid())
                    {
                        m_currentId = nativeId; // Сохраняем нативный ID
                        PropertyEngine::PrepareSelectionContext(nativeId.database(), { nativeId });
                    }
                }
                pAcadObj->Release();
            }
            return S_OK;
        }

        STDMETHODIMP Disconnect() { m_currentId = AcDbObjectId::kNull; return S_OK; }

        // МГНОВЕННЫЙ НАТИВНЫЙ ВЫВОД СТРОКИ ПАЛИТРЫ ИЗ std::map КЭША БЕЗ ТРАНЗАКЦИЙ
        STDMETHODIMP DynamicPropertyBridge::GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) {
            if (pVarData == nullptr) return E_POINTER;
            ::VariantInit(pVarData);
            AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
            if (!pDb) return E_FAIL;

            // ИСПРАВЛЕНО: Больше никакой конвертации строк, propName уже является std::wstring!
            if (m_propType == ArxBridge::PropType::IsDouble) {
                double val = 0.0;
                if (PropertyEngine::GetPropertyValue(pDb, m_currentId, m_nativePropName, val)) {
                    pVarData->vt = VT_R8; pVarData->dblVal = val; return S_OK;
                }
            }
            else if (m_propType == ArxBridge::PropType::IsInteger) {
                int val = 0;
                if (PropertyEngine::GetPropertyValue(pDb, m_currentId, m_nativePropName, val)) {
                    pVarData->vt = VT_I4; pVarData->lVal = val; return S_OK;
                }
            }
            return E_FAIL;
        }
      
        STDMETHODIMP SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) { return S_OK; }
    };
}
