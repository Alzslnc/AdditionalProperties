#pragma once
#include <Unknwn.h>

typedef LONG PROPCAT;

/// <summary>
/// Чисто нативный базовый класс COM-моста. Полностью изолирован от SDK в заголовке,
/// что исключает любые конфликты макросов и типов в смешанных (C++/CLI) модулях.
/// </summary>
class DynamicPropertyNativeBase : public IUnknown
{
protected:
    long m_refCount;

public:
    DynamicPropertyNativeBase();
    virtual ~DynamicPropertyNativeBase();

    // 🚀 СТЫКОВЫЕ ЧИСТО ВИРТУАЛЬНЫЕ МЕТОДЫ-СОБЫТИЯ ДЛЯ C++/CLI НАСЛЕДНИКА
    virtual void OnGetDisplayName(BSTR* pBSTRName) = 0;
    virtual void OnIsPropertyReadOnly(BOOL* bIsReadOnly) = 0;
    virtual void OnIsPropertyEnabled(::IUnknown* pUnk, BOOL* pbEnabled) = 0;
    virtual void OnGetCurrentValueType(VARTYPE* pVarType) = 0;
    virtual void OnConnect(::IUnknown* pSink) = 0; // Передаем сырой IUnknown на стыке модулей
    virtual void OnDisconnect() = 0;
    virtual void OnGetCurrentValueData(::IUnknown* pUnk, ::VARIANT* pVarData) = 0;
    virtual HRESULT OnSetCurrentValueData(::IUnknown* pUnk, const ::VARIANT varData) = 0;
    virtual void OnGetCategoryName(BSTR* pbstrName) = 0;
    virtual void OnGetDescription(BSTR* pBSTRDescription) = 0;

    // 🚀 Объявляем виртуальный нативный метод извлечения ID
    virtual AcDbObjectId GetObjectIdFromIUnknown(::IUnknown* pUnk);

    // --- Базовый IUnknown ---
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvRaw) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
};
