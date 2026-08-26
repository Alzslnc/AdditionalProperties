#include "OpmPaletteSubscription.h" // Подключаем наш нативный синглтон
#include <vcclr.h>
#include "ArxBridgeCore.h"

using namespace Autodesk::AutoCAD::ApplicationServices;

public ref class OpmManagedBridge : public System::IDisposable {
private:
    Autodesk::AutoCAD::ApplicationServices::ModelessOperationEventHandler^ m_pModelessHandler;
    // Сохраняем делегаты менеджера документов, чтобы гарантировать точную отписку
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocActivatedHandler;
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocDestroyedHandler;
    System::EventHandler^ m_pIdleHandler;

    bool m_isIdleSubscribed; // Флаг-защита от множественной подписки на Idle

public:
    OpmManagedBridge() {
        m_isIdleSubscribed = false;

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

        // Защита: подписываемся на Idle только один раз
        if (!m_isIdleSubscribed) {
            m_isIdleSubscribed = true;
            Application::Idle += m_pIdleHandler;
        }
    }

    void OnAutoCADIdle(System::Object^ sender, System::EventArgs^ e) {
        // Сразу снимаем флаг и отписываем сохраненный делегат
        m_isIdleSubscribed = false;
        Application::Idle -= m_pIdleHandler;

        const std::vector<::AcDbObjectId>* pNativeIds = ArxBridge::ArxBridgeCore::m_frameSelectedIds;
        ArxBridge::OpmPaletteSubscription::getInstance().NotifyModelessOperationEnded(pNativeIds);
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
            // Используем сохраненные делегаты
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
        // Отписываем Idle, если он остался активен в момент выгрузки
        if (m_isIdleSubscribed) {
            Application::Idle -= m_pIdleHandler;
            m_isIdleSubscribed = false;
        }

        DocumentCollection^ docMgr = Application::DocumentManager;
        if (docMgr != nullptr) {
            docMgr->DocumentActivated -= m_pDocActivatedHandler;
            docMgr->DocumentToBeDestroyed -= m_pDocDestroyedHandler;

            for each (Document ^ doc in docMgr) {
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
