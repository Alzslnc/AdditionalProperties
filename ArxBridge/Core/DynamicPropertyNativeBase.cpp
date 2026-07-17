#include "DynamicPropertyNativeBase.h"

#pragma warning(push)
#pragma warning(disable: 4278 4192)
#define STRINGIFY(x) #x
#define MACRO_TO_STRING(x) STRINGIFY(x)
#import MACRO_TO_STRING(OARX_TLB) no_namespace rename("Delete", "AcadDelete") rename("Block", "AcadBlock")
#pragma warning(pop)

// 1. Конструктор по умолчанию (Зеркальное совпадение с .h)
DynamicPropertyNativeBase::DynamicPropertyNativeBase()
    : m_refCount(1), m_currentId(AcDbObjectId::kNull), m_nativePropName(L""), m_nativeDescription(L"")
{}

// 2. Параметризованный конструктор
DynamicPropertyNativeBase::DynamicPropertyNativeBase(const wchar_t* name, const wchar_t* description)
    : m_refCount(1), m_currentId(AcDbObjectId::kNull), m_nativePropName(name), m_nativeDescription(description)
{}

DynamicPropertyNativeBase::~DynamicPropertyNativeBase()
{}

AcDbObjectId DynamicPropertyNativeBase::GetObjectIdFromIUnknown(IUnknown* pUnk) {
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

// Ручной COM-оркестратор со статик-кастом на честные родительские интерфейсы
STDMETHODIMP DynamicPropertyNativeBase::QueryInterface(REFIID riid, void** ppvRaw) {
    if (!ppvRaw) return E_POINTER;
    *ppvRaw = nullptr;

    if (riid == __uuidof(IUnknown) || riid.Data1 == 0x5F228E10) {
        *ppvRaw = static_cast<IDynamicProperty2*>(this);
        AddRef();
        return S_OK;
    }
    else if (riid == __uuidof(ICategorizeProperties)) {
        *ppvRaw = static_cast<ICategorizeProperties*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) DynamicPropertyNativeBase::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) DynamicPropertyNativeBase::Release() {
    long res = InterlockedDecrement(&m_refCount);
    if (res == 0) delete this;
    return res;
}

// --- Методы IDynamicProperty2 (Без ключевого слова override!) ---

STDMETHODIMP DynamicPropertyNativeBase::GetGUID(GUID* pPropGUID) {
    if (!pPropGUID) return E_POINTER;
    *pPropGUID = GUID_NULL;
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::GetDisplayName(BSTR* pBSTRName) {
    this->OnGetDisplayName(pBSTRName);
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::IsPropertyReadOnly(BOOL* bIsReadOnly) {
    this->OnIsPropertyReadOnly(bIsReadOnly);
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::GetDescription(BSTR* pBSTRDescription) {
    if (!pBSTRDescription) return E_POINTER;
    *pBSTRDescription = ::SysAllocString(this->m_nativeDescription.c_str());
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::IsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) {
    this->OnIsPropertyEnabled(pUnk, pbEnabled);
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::GetCurrentValueName(BSTR* pbstrName) {
    if (!pbstrName) return E_POINTER;
    *pbstrName = nullptr;
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::GetCurrentValueType(VARTYPE* pVarType) {
    this->OnGetCurrentValueType(pVarType);
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::Connect(IUnknown* pSink) {
    this->OnConnect(pSink);
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::Disconnect() {
    this->OnDisconnect();
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) {
    this->OnGetCurrentValueData(pUnk, pVarData);
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) {
    return this->OnSetCurrentValueData(pUnk, varData);
}

// --- Методы ICategorizeProperties ---

STDMETHODIMP DynamicPropertyNativeBase::MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) {
    if (!ppropcat) return E_POINTER;
    *ppropcat = 1;
    return S_OK;
}

STDMETHODIMP DynamicPropertyNativeBase::GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) {
    if (!pbstrName) return E_POINTER;
    if (propcat == 1) {
        std::wstring wGroup = OnGetCategoryName(m_currentId);
        *pbstrName = ::SysAllocString(wGroup.c_str());
        return S_OK;
    }
    return E_NOTIMPL;
}
