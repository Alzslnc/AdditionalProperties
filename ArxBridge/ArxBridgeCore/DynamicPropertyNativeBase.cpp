#include <windows.h>
#include <Unknwn.h>

// Подключаем оригинальные заголовочные файлы ObjectARX SDK
#include "arxHeaders.h"
//#include <dynprops.h>
//#include "dbxutil.h" 
#include "DynamicPropertyNativeBase.h"
#include "RealCOMProperty.h" 

// РАСПАКОВКА ТАЙПЛИБА AUTOCAD
#pragma warning(push)
#pragma warning(disable: 4278 4192)
#define STRINGIFY(x) #x
#define MACRO_TO_STRING(x) STRINGIFY(x)
#import MACRO_TO_STRING(OARX_TLB) no_namespace rename("Delete", "AcadDelete") rename("Block", "AcadBlock")
#pragma warning(pop)

// МЕТОД ИЗВЛЕЧЕНИЯ ID (Строго сразу после #import)
bool DynamicPropertyNativeBase::GetObjectIdFromIUnknown(::IUnknown* pUnk, ::AcDbObjectId* pOutId) {
    if (!pOutId) return false;
    *pOutId = ::AcDbObjectId::kNull;
    if (!pUnk) return false;

    IAcadObject* pAcadObj = nullptr;
    if (SUCCEEDED(pUnk->QueryInterface(__uuidof(IAcadObject), (void**)&pAcadObj))) {
        LONG_PTR rawId = 0;

        if (SUCCEEDED(pAcadObj->get_ObjectID(&rawId)) && rawId != 0) {
            pOutId->setFromOldId(rawId);
            pAcadObj->Release();
            return true;
        }
        pAcadObj->Release();
    }
    return false;
}

// Конструктор: инициализируем точный указатель в nullptr
DynamicPropertyNativeBase::DynamicPropertyNativeBase()
    : m_refCount(1), m_pRealProperty(nullptr)
{}

// Деструктор: честно очищаем память под плоским объектом RealCOMProperty
DynamicPropertyNativeBase::~DynamicPropertyNativeBase() {
    if (m_pRealProperty != nullptr) {
        delete m_pRealProperty; // 🚀 Теперь без static_cast, компилятор знает этот тип!
        m_pRealProperty = nullptr;
    }
}

// Фабричный QueryInterface: лениво создает единственный экземпляр плоского RealCOMProperty
STDMETHODIMP DynamicPropertyNativeBase::QueryInterface(REFIID riid, void** ppvRaw) {
    if (!ppvRaw) return E_POINTER;

    if (m_pRealProperty == nullptr) {
        m_pRealProperty = new RealCOMProperty(this); // 🚀 Создаем напрямую
    }

    // Делегируем запрос интерфейсов (IDynamicProperty2 или ICategorizeProperties) нашему сборщику
    return m_pRealProperty->QueryInterface(riid, ppvRaw);
}

ULONG __stdcall DynamicPropertyNativeBase::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG __stdcall DynamicPropertyNativeBase::Release() {
    long res = InterlockedDecrement(&m_refCount);
    if (res == 0) delete this;
    return res;
}

bool DynamicPropertyNativeBase::GetPropertyGUID(GUID* pGuid) {
    if (pGuid == nullptr) return false;

    // Если реальный объект существует, безопасно запрашиваем у него GUID
    if (m_pRealProperty != nullptr) {
        // Вызываем родной метод GetGUID у вашего скрытого класса-реализации
        if (SUCCEEDED(m_pRealProperty->GetGUID(pGuid))) {
            return true;
        }
    }
    return false;
}
