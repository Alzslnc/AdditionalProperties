#include "RealCOMProperty.h"
#include "DynamicPropertyNativeBase.h"

STDMETHODIMP RealCOMProperty::GetGUID(GUID* pPropGUID) {
    if (!pPropGUID) return E_POINTER;
    *pPropGUID = m_propertyGuid;
    return S_OK;
}

STDMETHODIMP RealCOMProperty::GetDisplayName(BSTR* pBSTRName) {
    m_pBridge->OnGetDisplayName(pBSTRName);
    return S_OK;
}

STDMETHODIMP RealCOMProperty::IsPropertyReadOnly(BOOL* bIsReadOnly) {
    m_pBridge->OnIsPropertyReadOnly(bIsReadOnly);
    return S_OK;
}

STDMETHODIMP RealCOMProperty::GetDescription(BSTR* pBSTRDescription) {
    m_pBridge->OnGetDescription(pBSTRDescription);
    return S_OK;
}

STDMETHODIMP RealCOMProperty::IsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) {
    m_pBridge->OnIsPropertyEnabled(pUnk, pbEnabled);
    return S_OK;
}

STDMETHODIMP RealCOMProperty::GetCurrentValueName(BSTR* pbstrName) {
    if (!pbstrName) return E_POINTER;
    *pbstrName = nullptr;
    return S_OK;
}

STDMETHODIMP RealCOMProperty::GetCurrentValueType(VARTYPE* pVarType) {
    m_pBridge->OnGetCurrentValueType(pVarType);
    return S_OK;
}

STDMETHODIMP RealCOMProperty::Connect(IDynamicPropertyNotify2* pSink) {
    // Приводим к IUnknown* через reinterpret_cast, чтобы не тащить тяжелые SDK-заголовки уведомлений
    m_pBridge->OnConnect(reinterpret_cast<IUnknown*>(pSink));
    return S_OK;
}

STDMETHODIMP RealCOMProperty::Disconnect() {
    m_pBridge->OnDisconnect();
    return S_OK;
}

STDMETHODIMP RealCOMProperty::GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) {
    m_pBridge->OnGetCurrentValueData(pUnk, pVarData);
    return S_OK;
}

STDMETHODIMP RealCOMProperty::SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) {
    return m_pBridge->OnSetCurrentValueData(pUnk, varData);
}
