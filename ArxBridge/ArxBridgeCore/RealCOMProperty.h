#pragma once
#include "OpmIDynamicProperty2.h"
#include "OpmICategorizeProperties.h"

// Опережающие объявления классов, чтобы заголовок был полностью автономным
class DynamicPropertyNativeBase;

/// <summary>
/// Единый плоский COM-класс реализации интерфейсов палитры свойств AutoCAD.
/// Логика методов физически разделена по разным .cpp файлам.
/// </summary>
class RealCOMProperty : public IDynamicProperty2, public ICategorizeProperties
{
private:
    DynamicPropertyNativeBase* m_pBridge;

public:
    RealCOMProperty(DynamicPropertyNativeBase* pBridge);
    virtual ~RealCOMProperty() = default;

    // === Базовый интерфейс IUnknown ===
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvRaw) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // === Интерфейс IDynamicProperty2 (Свойства) ===
    STDMETHODIMP GetGUID(GUID* pPropGUID) override;
    STDMETHODIMP GetDisplayName(BSTR* pBSTRName) override;
    STDMETHODIMP IsPropertyReadOnly(BOOL* bIsReadOnly) override;
    STDMETHODIMP GetDescription(BSTR* pBSTRDescription) override;
    STDMETHODIMP IsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) override;
    STDMETHODIMP GetCurrentValueName(BSTR* pbstrName) override;
    STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) override;
    STDMETHODIMP Connect(::IDynamicPropertyNotify2* pSink) override;
    STDMETHODIMP Disconnect() override;
    STDMETHODIMP GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) override;
    STDMETHODIMP SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) override;

    // === Интерфейс ICategorizeProperties (Категории) ===
    STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) override;
    STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) override;
};
