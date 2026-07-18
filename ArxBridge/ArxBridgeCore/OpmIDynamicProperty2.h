#pragma once
#include <Unknwn.h>

// Опережающее объявление для интерфейса уведомлений палитры
interface IDynamicPropertyNotify2;

// {9CAF41C2-CA86-4ffb-B05A-AC43C424D076}
#ifndef IDynamicProperty2_DEFINED
#define IDynamicProperty2_DEFINED
interface __declspec(uuid("9CAF41C2-CA86-4ffb-B05A-AC43C424D076")) IDynamicProperty2 : public IUnknown
{
public:
    // *** IDynamicProperty2 методы ***
    virtual STDMETHODIMP GetGUID(GUID* propGUID) = 0;
    virtual STDMETHODIMP GetDisplayName(BSTR* bstrName) = 0;
    virtual STDMETHODIMP IsPropertyEnabled(::IUnknown* pUnk, BOOL* pbEnabled) = 0;
    virtual STDMETHODIMP IsPropertyReadOnly(BOOL* pbReadonly) = 0;
    virtual STDMETHODIMP GetDescription(BSTR* bstrName) = 0;

    // *** Базовая информация о значении свойства ***
    virtual STDMETHODIMP GetCurrentValueName(BSTR* pbstrName) = 0;
    virtual STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) = 0;
    virtual STDMETHODIMP GetCurrentValueData(::IUnknown* pUnk, ::VARIANT* pvarData) = 0;
    virtual STDMETHODIMP SetCurrentValueData(::IUnknown* pUnk, const ::VARIANT varData) = 0;

    // *** Уведомления палитры (События) ***
    virtual STDMETHODIMP Connect(::IDynamicPropertyNotify2* pSink) = 0;
    virtual STDMETHODIMP Disconnect() = 0;
};
#endif
