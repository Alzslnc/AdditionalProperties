#include "OpmPaletteSubscription.h" // Подключаем наш нативный синглтон
#include <vcclr.h>
#include "ArxBridgeCore.h"
#include "DocumentCacheManager.h"

using namespace Autodesk::AutoCAD::ApplicationServices;

public ref class OpmManagedBridge : public System::IDisposable {
private:
    Autodesk::AutoCAD::ApplicationServices::ModelessOperationEventHandler^ m_pModelessHandler;
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocActivatedHandler;
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocDestroyedHandler;
    System::EventHandler^ m_pIdleHandler;
    // Флаг защиты от множественной подписки на Idle
    bool m_isIdleSubscribed;

public:
    OpmManagedBridge() {
        m_isIdleSubscribed = false;

        // Инициализируем делегаты
        m_pModelessHandler = gcnew ModelessOperationEventHandler(this, &OpmManagedBridge::OnModelessOperationEnded);
        m_pDocActivatedHandler = gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentActivated);
        m_pDocDestroyedHandler = gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentToBeDestroyed);
        m_pIdleHandler = gcnew System::EventHandler(this, &OpmManagedBridge::OnAutoCADIdle);
    }

    // Деструктор (Реализация IDisposable в C++/CLI)
    ~OpmManagedBridge() {
        Unsubscribe();
    }

    void OnModelessOperationEnded(System::Object^ sender, ModelessOperationEventArgs^ e) {
        if (ArxBridge::ArxBridgeCore::m_frameSelectedIds == nullptr) return;

        // Цепляемся за Idle. Пока пользователь крутит комбобокс, Idle будет срабатывать,
        // но метод OnAutoCADIdle быстро отфильтрует эти вызовы по флагу кэша.
        if (!m_isIdleSubscribed) {
            m_isIdleSubscribed = true;
            Autodesk::AutoCAD::ApplicationServices::Application::Idle += m_pIdleHandler;
        }
    }

    void OnAutoCADIdle(System::Object^ sender, System::EventArgs^ e) {
        // 1. Сразу же отписываемся от текущего тика Idle
        m_isIdleSubscribed = false;
        Autodesk::AutoCAD::ApplicationServices::Application::Idle -= m_pIdleHandler;

        // 2. ВЫСОКОСКОРОСТНАЯ ПРОВЕРКА НА ПРЕВЬЮ:
        // ПРЯМАЯ ПРОВЕРКА ЧЕРЕЗ СИНГЛТОН ПАЛИТРЫ:
        // Быстро проверяем, взвел ли реактор флаг изменений в подписке палитры
        if (!ArxBridge::OpmPaletteSubscription::getInstance().m_isCacheDirty) {
            return;
        }

        // 3. Если кэш действительно «грязный» — значит, коммит реальной транзакции выполнен.
        try {
            // Извлекаем нативный указатель (валидность гарантирована предыдущим шагом перед подпиской)
            const std::vector<::AcDbObjectId>* pNativeIds = ArxBridge::ArxBridgeCore::m_frameSelectedIds;

            // Безопасно пинаем палитру для финального обновления
            ArxBridge::OpmPaletteSubscription::getInstance().NotifyModelessOperationEnded(pNativeIds);
        }
        finally {
            // 4. СБРОС ФЛАГА БЛОКИРОВКИ:
            // В самом конце конвейера переводим кэш обратно в «чистое» состояние (false).
            // Это защитит палитру от ложных обновлений при последующих наведениях мыши.
            ArxBridge::OpmPaletteSubscription::getInstance().m_isCacheDirty = false;
        }
    }

    void OnDocumentActivated(System::Object^ sender, DocumentCollectionEventArgs^ e) {
        if (e->Document != nullptr) {
            e->Document->ModelessOperationEnded::remove(m_pModelessHandler);
            e->Document->ModelessOperationEnded::add(m_pModelessHandler);
        }
    }

    void OnDocumentToBeDestroyed(System::Object^ sender, DocumentCollectionEventArgs^ e) {
        if (e->Document != nullptr) {
            e->Document->ModelessOperationEnded::remove(m_pModelessHandler);
        }
    }

    void Subscribe() {
        DocumentCollection^ docMgr = Application::DocumentManager;
        if (docMgr != nullptr) {
            docMgr->DocumentActivated += m_pDocActivatedHandler;
            docMgr->DocumentToBeDestroyed += m_pDocDestroyedHandler;

            Document^ pActiveDoc = docMgr->MdiActiveDocument;
            if (pActiveDoc != nullptr) {
                pActiveDoc->ModelessOperationEnded::remove(m_pModelessHandler);
                pActiveDoc->ModelessOperationEnded::add(m_pModelessHandler);
            }
        }
    }

    void Unsubscribe() {
        if (m_isIdleSubscribed) {
            Autodesk::AutoCAD::ApplicationServices::Application::Idle -= m_pIdleHandler;
            m_isIdleSubscribed = false;
        }

        DocumentCollection^ docMgr = Application::DocumentManager;
        if (docMgr != nullptr) {
            docMgr->DocumentActivated -= m_pDocActivatedHandler;
            docMgr->DocumentToBeDestroyed -= m_pDocDestroyedHandler;

            for each(Document ^ doc in docMgr) {
                if (doc != nullptr && !doc->IsDisposed) {
                    doc->ModelessOperationEnded::remove(m_pModelessHandler);
                }
            }
        }
    }
};


// 2. Управляемая точка входа (IExtensionApplication)
public ref class ExtensionApplication : public Autodesk::AutoCAD::Runtime::IExtensionApplication {
private:
    OpmManagedBridge^ m_bridge;

public:
    virtual void Initialize() {      
        // Запускаем только управляемый мост-посредник для палитры
        m_bridge = gcnew OpmManagedBridge();
        m_bridge->Subscribe();
    }

    virtual void Terminate() {
        if (m_bridge != nullptr) {
            delete m_bridge; // Вызывает деструктор (~OpmManagedBridge), который гарантированно отпишет всё
            m_bridge = nullptr;
        }    
    }
};
