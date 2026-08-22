#include "OpmPaletteSubscription.h" // Подключаем наш нативный синглтон
#include <vcclr.h>
#include "ArxBridgeCore.h"

using namespace Autodesk::AutoCAD::ApplicationServices;

public ref class OpmManagedBridge {
private:
    // ИСПРАВЛЕНО 1: Указали точный тип делегата, который требует AutoCAD [4]
    Autodesk::AutoCAD::ApplicationServices::ModelessOperationEventHandler^ m_pModelessHandler;

public:
    OpmManagedBridge() {
        // ИСПРАВЛЕНО 2: Создаем делегат правильного типа [4]
        m_pModelessHandler = gcnew Autodesk::AutoCAD::ApplicationServices::ModelessOperationEventHandler(
            this, &OpmManagedBridge::OnModelessOperationEnded);
    }

    void OnModelessOperationEnded(System::Object^ sender, Autodesk::AutoCAD::ApplicationServices::ModelessOperationEventArgs^ e) {

        if (ArxBridge::ArxBridgeCore::m_frameSelectedIds == nullptr) return;

        // Вместо прямого вызова просто подписываемся на событие Idle.
        // Как только палитра закончит ввод, а AutoCAD завершит коммит транзакции, 
        // управление мгновенно перейдет в OnAutoCADIdle.
        Autodesk::AutoCAD::ApplicationServices::Application::Idle +=
            gcnew System::EventHandler(this, &OpmManagedBridge::OnAutoCADIdle);       
    }

    void OnAutoCADIdle(System::Object^ sender, System::EventArgs^ e) {
        // 1. Сразу же отписываемся, чтобы это сработало строго один раз!
        Autodesk::AutoCAD::ApplicationServices::Application::Idle -=
            gcnew System::EventHandler(this, &OpmManagedBridge::OnAutoCADIdle);

        // 2. Извлекаем сохраненные ID (как вы и делали)
        const std::vector<::AcDbObjectId>* pNativeIds = ArxBridge::ArxBridgeCore::m_frameSelectedIds;

        // 3. Безопасно пинаем палитру — теперь база данных гарантированно свободна
        ArxBridge::OpmPaletteSubscription::getInstance().NotifyModelessOperationEnded(pNativeIds);
    }

    void OnDocumentActivated(System::Object^ sender, DocumentCollectionEventArgs^ e) {
        if (e->Document != nullptr) {
            // ИСПРАВЛЕНО 3: Используем методы remove() и add() вместо -= и +=
            // Это стандартное требование C++/CLI для подписки на события внешних управляемых сборок
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
            docMgr->DocumentActivated += gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentActivated);
            docMgr->DocumentToBeDestroyed += gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentToBeDestroyed);

            Document^ pActiveDoc = docMgr->MdiActiveDocument;
            if (pActiveDoc != nullptr) {
                pActiveDoc->ModelessOperationEnded::remove(m_pModelessHandler);
                pActiveDoc->ModelessOperationEnded::add(m_pModelessHandler);
            }
        }
    }

    void Unsubscribe() {
        DocumentCollection^ docMgr = Application::DocumentManager;
        if (docMgr != nullptr) {
            docMgr->DocumentActivated -= gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentActivated);
            docMgr->DocumentToBeDestroyed -= gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentToBeDestroyed);

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
        // Снимаем управляемые .NET подписки с документов
        if (m_bridge != nullptr) {
            m_bridge->Unsubscribe();
            m_bridge = nullptr;
        }
    }
};
