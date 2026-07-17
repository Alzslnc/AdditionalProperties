#pragma once
#include <Unknwn.h>
#include <string>
#include "dbid.h" // Для AcDbObjectId

typedef LONG PROPCAT;

// Вручную объявляем интерфейс категорий палитры ActiveX / OPM
#ifndef ICategorizeProperties_DEFINED
#define ICategorizeProperties_DEFINED
interface __declspec(uuid("4D07FC10-F9D7-11ce-B196-00AA00B914D5")) ICategorizeProperties : public IUnknown
{
public:
    virtual STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) = 0;
    virtual STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) = 0;
};
#endif

// Вручную объявляем интерфейс IDynamicProperty2 из ObjectARX SDK с точными типами COM
#ifndef IDynamicProperty2_DEFINED
#define IDynamicProperty2_DEFINED
interface __declspec(uuid("5F228E10-53D2-473d-9C21-BC20D00C4465")) IDynamicProperty2 : public IUnknown
{
public:
    virtual STDMETHODIMP GetGUID(GUID* pPropGUID) = 0;
    virtual STDMETHODIMP GetDisplayName(BSTR* pBSTRName) = 0;
    virtual STDMETHODIMP IsPropertyReadOnly(BOOL* bIsReadOnly) = 0;
    virtual STDMETHODIMP GetDescription(BSTR* pBSTRDescription) = 0;
    virtual STDMETHODIMP IsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) = 0;
    virtual STDMETHODIMP GetCurrentValueName(BSTR* pbstrName) = 0;
    virtual STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) = 0;

    // 🚀 СИНХРОНИЗИРОВАНО: Указан чистый тип IUnknown*, чтобы сигнатура 
    // идеально совпала во всех трех модулях нашего проекта!
    virtual STDMETHODIMP Connect(IUnknown* pSink) = 0;

    virtual STDMETHODIMP Disconnect() = 0;
    virtual STDMETHODIMP GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) = 0;
    virtual STDMETHODIMP SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) = 0;
};
#endif

/// <summary>
/// Чисто нативный базовый класс COM-моста. Избавлен от CLR-контекста,
/// что гарантирует идеальное бинарное выравнивание памяти для acpi.arx.
/// </summary>
class DynamicPropertyNativeBase :
    public IDynamicProperty2,
    public ICategorizeProperties
{
protected:
    long m_refCount;
    AcDbObjectId m_currentId;
    std::wstring m_nativePropName;
    std::wstring m_nativeDescription;

    // Вспомогательный метод извлечения AcDbObjectId из IUnknown палитры AutoCAD
    AcDbObjectId GetObjectIdFromIUnknown(IUnknown* pUnk);

public:
    // Конструкторы жизненного цикла
    DynamicPropertyNativeBase();
    DynamicPropertyNativeBase(const wchar_t* name, const wchar_t* description);
    virtual ~DynamicPropertyNativeBase();

    // 🚀 СТЫКОВЫЕ ЧИСТО ВИРТУАЛЬНЫЕ МЕТОДЫ-СОБЫТИЯ ДЛЯ С++/CLI НАСЛЕДНИКА
    virtual void OnGetDisplayName(BSTR* pBSTRName) = 0;
    virtual void OnIsPropertyReadOnly(BOOL* bIsReadOnly) = 0;
    virtual void OnIsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) = 0;
    virtual void OnGetCurrentValueType(VARTYPE* pVarType) = 0;
    virtual void OnConnect(IUnknown* pSink) = 0;
    virtual void OnDisconnect() = 0;
    virtual void OnGetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) = 0;
    virtual HRESULT OnSetCurrentValueData(IUnknown* pUnk, const VARIANT varData) = 0;
    virtual std::wstring OnGetCategoryName(AcDbObjectId id) = 0;

    // --- Реализация методов IDynamicProperty2 ---
    virtual STDMETHODIMP GetGUID(GUID* pPropGUID) override;
    virtual STDMETHODIMP GetDisplayName(BSTR* pBSTRName) override;
    virtual STDMETHODIMP IsPropertyReadOnly(BOOL* bIsReadOnly) override;
    virtual STDMETHODIMP GetDescription(BSTR* pBSTRDescription) override;
    virtual STDMETHODIMP IsPropertyEnabled(IUnknown* pUnk, BOOL* pbEnabled) override;
    virtual STDMETHODIMP GetCurrentValueName(BSTR* pbstrName) override;
    virtual STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) override;
    virtual STDMETHODIMP Connect(IUnknown* pSink) override;
    virtual STDMETHODIMP Disconnect() override;
    virtual STDMETHODIMP GetCurrentValueData(IUnknown* pUnk, VARIANT* pVarData) override;
    virtual STDMETHODIMP SetCurrentValueData(IUnknown* pUnk, const VARIANT varData) override;

    // --- Реализация методов ICategorizeProperties ---
    virtual STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) override;
    virtual STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) override;

    // --- Базовый IUnknown ---
    virtual STDMETHODIMP QueryInterface(REFIID riid, void** ppvRaw) override;
    virtual STDMETHODIMP_(ULONG) AddRef() override;
    virtual STDMETHODIMP_(ULONG) Release() override;
};
