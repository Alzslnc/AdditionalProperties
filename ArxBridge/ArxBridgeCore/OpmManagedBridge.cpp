#include "OpmPaletteSubscription.h" // Подключаем наш нативный синглтон
#include <vcclr.h>
#include "ArxBridgeCore.h"

using namespace Autodesk::AutoCAD::ApplicationServices;

public ref class OpmManagedBridge : public System::IDisposable {
private:
    Autodesk::AutoCAD::ApplicationServices::ModelessOperationEventHandler^ m_pModelessHandler;
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocActivatedHandler;
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocDestroyedHandler;
    System::EventHandler^ m_pQuiescentHandler; 
    bool m_isQuiescentSubscribed; 

public:
    OpmManagedBridge() {
        m_isQuiescentSubscribed = false;

        m_pModelessHandler = gcnew ModelessOperationEventHandler(this, &OpmManagedBridge::OnModelessOperationEnded);
        m_pDocActivatedHandler = gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentActivated);
        m_pDocDestroyedHandler = gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentToBeDestroyed);
        m_pQuiescentHandler = gcnew System::EventHandler(this, &OpmManagedBridge::OnAutoCADQuiescent);     
    }

    // Деструктор (Реализация IDisposable в C++/CLI)
    ~OpmManagedBridge() {
        Unsubscribe();
    }

    void OnModelessOperationEnded(System::Object^ sender, ModelessOperationEventArgs^ e) {
        if (ArxBridge::ArxBridgeCore::m_frameSelectedIds == nullptr) return;

        // Подписываемся на состояние покоя редактора текущего активного документа.
        // Пока комбобокс открыт, событие не наступит, защищая UI от сброса превью.
        Document^ pActiveDoc = Application::DocumentManager->MdiActiveDocument;
        if (pActiveDoc != nullptr && pActiveDoc->Editor != nullptr) {
            if (!m_isQuiescentSubscribed) {
                m_isQuiescentSubscribed = true;
                pActiveDoc->Editor->EnteringQuiescentState += m_pQuiescentHandler;
            }
        }
    }
    void OnAutoCADQuiescent(System::Object^ sender, System::EventArgs^ e) {
        // Сразу же снимаем флаг и отписываемся от редактора
        m_isQuiescentSubscribed = false;

        Document^ pActiveDoc = Application::DocumentManager->MdiActiveDocument;
        if (pActiveDoc != nullptr && pActiveDoc->Editor != nullptr) {
            pActiveDoc->Editor->EnteringQuiescentState -= m_pQuiescentHandler;
        }

        // Выбор в комбобоксе гарантированно зафиксирован, безопасно пинаем палитру
        if (ArxBridge::ArxBridgeCore::m_frameSelectedIds != nullptr) {
            const std::vector<::AcDbObjectId>* pNativeIds = ArxBridge::ArxBridgeCore::m_frameSelectedIds;
            ArxBridge::OpmPaletteSubscription::getInstance().NotifyModelessOperationEnded(pNativeIds);
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

            // Если уничтожается документ, на покой которого мы были подписаны — сбрасываем флаг
            if (m_isQuiescentSubscribed && Application::DocumentManager->MdiActiveDocument == e->Document) {
                if (e->Document->Editor != nullptr) {
                    e->Document->Editor->EnteringQuiescentState -= m_pQuiescentHandler;
                }
                m_isQuiescentSubscribed = false;
            }
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
        // 1. Безопасно отписываемся от покоя ТОЛЬКО активного документа (где подписка реально могла быть)
        if (m_isQuiescentSubscribed) {
            Document^ pActiveDoc = Application::DocumentManager->MdiActiveDocument;
            if (pActiveDoc != nullptr && pActiveDoc->Editor != nullptr) {
                pActiveDoc->Editor->EnteringQuiescentState -= m_pQuiescentHandler;
            }
            m_isQuiescentSubscribed = false; // Сбрасываем флаг ОДИН раз вне всяких циклов
        }

        // 2. А вот здесь уже спокойно чистим менеджер и коллекции документов
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
