#include <windows.h>
#include <Unknwn.h>

// 1. Подключаем оригинальные заголовочные файлы ObjectARX SDK
#include "arxHeaders.h"
#include <dynprops.h>
#include "dbxutil.h" 
#include "DynamicPropertyNativeBase.h"

// 🚀 ВОЗВРАЩАЕМ РАСПАКОВКУ ТАЙПЛИБА AUTOCAD!
// Это сгенерирует типы и сделает интерфейс IAcadObject видимым для компилятора
#pragma warning(push)
#pragma warning(disable: 4278 4192)
#define STRINGIFY(x) #x
#define MACRO_TO_STRING(x) STRINGIFY(x)
#import MACRO_TO_STRING(OARX_TLB) no_namespace rename("Delete", "AcadDelete") rename("Block", "AcadBlock")
#pragma warning(pop)

// Ручное объявление интерфейса категорий (так как в dynprops.h его нет)
#ifndef ICategorizeProperties_DEFINED
#define ICategorizeProperties_DEFINED
interface __declspec(uuid("4D07FC10-F9D7-11ce-B196-00AA00B914D5")) ICategorizeProperties : public IUnknown
{
public:
    virtual STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) = 0;
    virtual STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) = 0;
};
#endif

AcDbObjectId DynamicPropertyNativeBase::GetObjectIdFromIUnknown(::IUnknown* pUnk) {
    AcDbObjectId id = AcDbObjectId::kNull;
    if (!pUnk) return id;

    IAcadObject* pAcadObj = nullptr;
    // В нативном модуле __uuidof(IAcadObject) из #import сработает бинарно безупречно
    if (SUCCEEDED(pUnk->QueryInterface(__uuidof(IAcadObject), (void**)&pAcadObj))) {
        LONG_PTR rawId = 0;

        // Запрашиваем ID. Благодаря раннему связыванию компилятора, промахнуться мимо vtable невозможно
        if (SUCCEEDED(pAcadObj->get_ObjectID(&rawId)) && rawId != 0) {
            id.setFromOldId(rawId);
        }
        pAcadObj->Release();
    }
    return id;
}

// 🚀 НАСТОЯЩАЯ МАТРЁШКА: Настоящий COM-класс. acpi.arx примет его структуру как родную!
class RealCOMProperty : public ::IDynamicProperty2, public ::ICategorizeProperties
{
private:
    DynamicPropertyNativeBase* m_pBridge;

public:
    RealCOMProperty(DynamicPropertyNativeBase* pBridge) : m_pBridge(pBridge) {}

    // Реализация IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvRaw) override {
        if (!ppvRaw) return E_POINTER;
        if (riid == __uuidof(::IUnknown) || riid == __uuidof(::IDynamicProperty2)) {
            *ppvRaw = static_cast<::IDynamicProperty2*>(this);
            m_pBridge->AddRef(); return S_OK;
        }
        if (riid == __uuidof(::ICategorizeProperties)) {
            *ppvRaw = static_cast<::ICategorizeProperties*>(this);
            m_pBridge->AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return m_pBridge->AddRef(); }
    STDMETHODIMP_(ULONG) Release() override { return m_pBridge->Release(); }

    // Перенаправление вызовов палитры AutoCAD в виртуальные события моста
    STDMETHODIMP GetGUID(GUID* pPropGUID) override { if (!pPropGUID) return E_POINTER; *pPropGUID = GUID_NULL; return S_OK; }
    STDMETHODIMP GetDisplayName(BSTR* pBSTRName) override { m_pBridge->OnGetDisplayName(pBSTRName); return S_OK; }
    STDMETHODIMP IsPropertyReadOnly(BOOL* bIsReadOnly) override { m_pBridge->OnIsPropertyReadOnly(bIsReadOnly); return S_OK; }
    STDMETHODIMP GetDescription(BSTR* pBSTRDescription) override { m_pBridge->OnGetDescription(pBSTRDescription); return S_OK; }
    STDMETHODIMP IsPropertyEnabled(::IUnknown* pUnk, BOOL* pbEnabled) override { m_pBridge->OnIsPropertyEnabled(pUnk, pbEnabled); return S_OK; }
    STDMETHODIMP GetCurrentValueName(BSTR* pbstrName) override { if (!pbstrName) return E_POINTER; *pbstrName = nullptr; return S_OK; }
    STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) override { m_pBridge->OnGetCurrentValueType(pVarType); return S_OK; }

    // Connect на стыке принимает нативный IDynamicPropertyNotify2*, но мы пробрасываем его как IUnknown*
    STDMETHODIMP Connect(::IDynamicPropertyNotify2* pSink) override { m_pBridge->OnConnect(static_cast<::IUnknown*>(pSink)); return S_OK; }

    STDMETHODIMP Disconnect() override { m_pBridge->OnDisconnect(); return S_OK; }
    STDMETHODIMP GetCurrentValueData(::IUnknown* pUnk, ::VARIANT* pVarData) override { m_pBridge->OnGetCurrentValueData(pUnk, pVarData); return S_OK; }
    STDMETHODIMP SetCurrentValueData(::IUnknown* pUnk, const ::VARIANT varData) override { return m_pBridge->OnSetCurrentValueData(pUnk, varData); }

    // Реализация категорий палитры
    STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) override { if (!ppropcat) return E_POINTER; *ppropcat = 1; return S_OK; }
    STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) override {
        if (!pbstrName) return E_POINTER;
        if (propcat == 1) { m_pBridge->OnGetCategoryName(pbstrName); return S_OK; }
        return E_NOTIMPL;
    }
};

DynamicPropertyNativeBase::DynamicPropertyNativeBase() : m_refCount(1) {}
DynamicPropertyNativeBase::~DynamicPropertyNativeBase() {}

// Фабричный QueryInterface: выдает наружу честный RealCOMProperty
STDMETHODIMP DynamicPropertyNativeBase::QueryInterface(REFIID riid, void** ppvRaw) {
    if (!ppvRaw) return E_POINTER;

    RealCOMProperty* pRealProperty = new RealCOMProperty(this);
    HRESULT hr = pRealProperty->QueryInterface(riid, ppvRaw);
    if (FAILED(hr)) {
        delete pRealProperty;
    }
    return hr; // Наружу уходит указатель на честный IDynamicProperty2!
}

ULONG __stdcall DynamicPropertyNativeBase::AddRef() { return InterlockedIncrement(&m_refCount); }
ULONG __stdcall DynamicPropertyNativeBase::Release() {
    long res = InterlockedDecrement(&m_refCount);
    if (res == 0) delete this;
    return res;
}
