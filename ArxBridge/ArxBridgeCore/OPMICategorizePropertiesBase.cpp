#include "RealCOMProperty.h"
#include "DynamicPropertyNativeBase.h"

STDMETHODIMP RealCOMProperty::MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) {
    if (!ppropcat) return E_POINTER;
    // Передаем управление в мост
    m_pBridge->OnMapPropertyToCategory(reinterpret_cast<LONG*>(ppropcat));
    return S_OK;
}

STDMETHODIMP RealCOMProperty::GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) {
    if (!pbstrName) return E_POINTER;
    // Передаем управление в мост
    m_pBridge->OnGetCategoryName(pbstrName);
    return S_OK;
}
