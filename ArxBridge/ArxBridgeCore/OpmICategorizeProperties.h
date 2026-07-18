#pragma once
#include <Unknwn.h>

typedef LONG PROPCAT;

#ifndef ICategorizeProperties_DEFINED
#define ICategorizeProperties_DEFINED
interface __declspec(uuid("4D07FC10-F931-11ce-B001-00AA006884E5")) ICategorizeProperties : public IUnknown
{
public:
    virtual STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) = 0;
    virtual STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) = 0;
};
#endif
