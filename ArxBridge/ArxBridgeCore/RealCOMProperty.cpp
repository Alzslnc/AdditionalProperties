#include "RealCOMProperty.h"
#include "DynamicPropertyNativeBase.h"

// Конструктор: запоминаем указатель на нативный базовый мост
RealCOMProperty::RealCOMProperty(DynamicPropertyNativeBase* pBridge)
    : m_pBridge(pBridge)
{}

/// <summary>
/// Главный диспетчер интерфейсов COM. Отвечает на запросы AutoCAD
/// и выдает правильный бинарный указатель на vtable нужной ветки.
/// </summary>
STDMETHODIMP RealCOMProperty::QueryInterface(REFIID riid, void** ppvRaw) {
    if (!ppvRaw) return E_POINTER;

    // Если AutoCAD запрашивает базовый IUnknown или интерфейс свойств
    if (riid == __uuidof(::IUnknown) || riid == __uuidof(IDynamicProperty2)) {
        *ppvRaw = static_cast<IDynamicProperty2*>(this);
        m_pBridge->AddRef(); // Увеличиваем счетчик ссылок через мост
        return S_OK;
    }

    // Если AutoCAD запрашивает интерфейс категорий (группировки)
    if (riid == __uuidof(ICategorizeProperties)) {
        *ppvRaw = static_cast<ICategorizeProperties*>(this);
        m_pBridge->AddRef(); // Увеличиваем счетчик ссылок через мост
        return S_OK;
    }

    // Если запрошен интерфейс, который мы не поддерживаем
    *ppvRaw = nullptr;
    return E_NOINTERFACE;
}

// Перенаправляем подсчет ссылок напрямую на атомарные счетчики базового моста
STDMETHODIMP_(ULONG) RealCOMProperty::AddRef() {
    return m_pBridge->AddRef();
}

STDMETHODIMP_(ULONG) RealCOMProperty::Release() {
    return m_pBridge->Release();
}
