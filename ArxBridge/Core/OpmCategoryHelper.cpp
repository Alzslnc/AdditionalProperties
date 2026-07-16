#include <windows.h>

#ifndef ICategorizeProperties_DEFINED
#define ICategorizeProperties_DEFINED
typedef LONG PROPCAT;
interface __declspec(uuid("4D07FC10-F9D7-11ce-B196-00AA00B914D5")) ICategorizeProperties : public IUnknown {
public:
    virtual STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) = 0;
    virtual STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) = 0;
};
#endif

// Объявляем сигнатуру обратного вызова (колбека) в управляемый код
typedef HRESULT(__stdcall* GetCategoryNameCallback)(void* managedHandle, PROPCAT propcat, BSTR* pbstrName);

// Нативный прокси-класс
class CategoryProxy : public ICategorizeProperties
{
private:
    long m_refCount;
    void* m_managedHandle;               // Указатель на GC-хэндл вашего CLR-класса
    GetCategoryNameCallback m_callback;  // Функция, ведущая в управляемый код

public:
    CategoryProxy(void* managedHandle, GetCategoryNameCallback callback)
        : m_refCount(1), m_managedHandle(managedHandle), m_callback(callback) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (riid == __uuidof(ICategorizeProperties) || riid == __uuidof(IUnknown)) {
            *ppvObject = static_cast<ICategorizeProperties*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        long res = InterlockedDecrement(&m_refCount);
        if (res == 0) delete this;
        return res;
    }

    // Логика сопоставления ID свойства с категорией (обычно фиксированная)
    STDMETHODIMP MapPropertyToCategory(DISPID dispID, PROPCAT* ppropcat) override {
        if (!ppropcat) return E_POINTER;
        *ppropcat = 1; // Все свойства будут в категории с ID = 1
        return S_OK;
    }

    // А вот за именем категории мы идем в управляемый код через колбек
    STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID lcid, BSTR* pbstrName) override {
        if (!m_callback || !m_managedHandle) return E_FAIL;
        return m_callback(m_managedHandle, propcat, pbstrName);
    }
};

// Экспортируемая функция для создания прокси
extern "C" __declspec(dllexport) IUnknown* CreateCategoryProxy(void* managedHandle, GetCategoryNameCallback callback) {
    return new CategoryProxy(managedHandle, callback);
}
