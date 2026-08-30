#include "OpmPaletteSubscription.h" // Подключаем наш нативный синглтон
#include <vcclr.h>
#include "ArxBridgeCore.h"
#include "DocumentCacheManager.h"
#include "OpmTriggerState.h" // Подключаем наш нативный глушитель спама
#include "OpmMessageHook.h"  // Подключаем нативный Win32-хук

using namespace Autodesk::AutoCAD::ApplicationServices;

// Глобальные переменные для управления подпиской на Idle в рамках файла
static bool m_isIdleSubscribed = false;

// Объявляем нативный указатель на хэндлер Idle для вызова из нативных функций-мостов
static gcroot<System::EventHandler^> g_pIdleHandlerStatic = nullptr;

// Единый метод для безопасного планирования отложенного обновления в UI-потоке AutoCAD
static void PlanIdleUpdateGlobal() {
    // Для проверки gcroot используем .operator->()
    if (!m_isIdleSubscribed && g_pIdleHandlerStatic.operator->() != nullptr) {
        m_isIdleSubscribed = true;

        // Приведение типов из gcroot в обычный управляемый дескриптор при подписке
        System::EventHandler^ idleHandler = g_pIdleHandlerStatic;
        Autodesk::AutoCAD::ApplicationServices::Application::Idle += idleHandler;
    }
}

// НАЙТИВНЫЕ ГЛОБАЛЬНЫЕ ФУНКЦИИ-МОСТЫ (__cdecl)
void __cdecl GlobalTimerBridgeCallback() {
    // Вызывается нативным таймером, если за 50 мс замок комбобокса его НЕ потушил
    PlanIdleUpdateGlobal();
}

void __cdecl GlobalComboHookCallback() {
    // Вызывается Win32-хуком строго при физическом клике по комбобоксу (код 1)
    PlanIdleUpdateGlobal();
}

public ref class OpmManagedBridge : public System::IDisposable {
private:
    // ИСПРАВЛЕНО: Управляемые дескрипторы перенесены из глобальной области в поля класса
    Autodesk::AutoCAD::ApplicationServices::ModelessOperationEventHandler^ m_pModelessHandler;
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocActivatedHandler;
    Autodesk::AutoCAD::ApplicationServices::DocumentCollectionEventHandler^ m_pDocDestroyedHandler;
    System::EventHandler^ m_pIdleHandler;

public:
    OpmManagedBridge() {
        m_isIdleSubscribed = false;

        // Инициализируем делегаты внутри конструктора экземпляра
        m_pModelessHandler = gcnew ModelessOperationEventHandler(this, &OpmManagedBridge::OnModelessOperationEnded);
        m_pDocActivatedHandler = gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentActivated);
        m_pDocDestroyedHandler = gcnew DocumentCollectionEventHandler(this, &OpmManagedBridge::OnDocumentToBeDestroyed);
        m_pIdleHandler = gcnew System::EventHandler(this, &OpmManagedBridge::OnAutoCADIdle);

        // Сохраняем ссылку на обработчик Idle в глобальный статический указатель для нативных мостов
        g_pIdleHandlerStatic = m_pIdleHandler;
    }

    // Деструктор (Реализация IDisposable)
    ~OpmManagedBridge() {
        Unsubscribe();
        g_pIdleHandlerStatic = nullptr;
        m_pModelessHandler = nullptr;
        m_pDocActivatedHandler = nullptr;
        m_pDocDestroyedHandler = nullptr;
        m_pIdleHandler = nullptr;
    }

    // 1. ВЕТКА ТЕКСТБОКСОВ: Точка ModelessOperationEnded
    void OnModelessOperationEnded(System::Object^ sender, ModelessOperationEventArgs^ e) {
        if (ArxBridge::ArxBridgeCore::m_frameSelectedIds == nullptr) return;

        // Перезапускаем нативный Win32-таймер ожидания на 50 мс
        OpmTriggerState::ProcessModelessEnded();
    }

    // 2. ЕДИНЫЙ ОБРАБОТЧИК IDLE
    void OnAutoCADIdle(System::Object^ sender, System::EventArgs^ e) {
        m_isIdleSubscribed = false;
        Autodesk::AutoCAD::ApplicationServices::Application::Idle -= m_pIdleHandler;

        const std::vector<::AcDbObjectId>* pNativeIds = ArxBridge::ArxBridgeCore::m_frameSelectedIds;

        // Передаем управление в нативный синглтон палитры свойств
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
        if (m_isIdleSubscribed && m_pIdleHandler != nullptr) {
            Autodesk::AutoCAD::ApplicationServices::Application::Idle -= m_pIdleHandler;
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


// 3. Управляемая точка входа (IExtensionApplication)
public ref class ExtensionApplication : public Autodesk::AutoCAD::Runtime::IExtensionApplication {
private:
    OpmManagedBridge^ m_bridge;

public:
    virtual void Initialize() {
        // ИСПРАВЛЕНО: Передаем верное имя функции GlobalComboHookCallback вместо несуществующего Bridge
        OpmTriggerState::Initialize(&GlobalTimerBridgeCallback);
        ArxBridge::OpmPaletteSubscription::RegisterComboBoxCallback(&GlobalComboHookCallback);

        // Инициализируем Win32-хук сообщений Windows строго для перехвата комбобоксов
        OpmMessageHook::Start(&GlobalComboHookCallback);

        // Запускаем управляемый мост-посредник для палитры
        m_bridge = gcnew OpmManagedBridge();
        m_bridge->Subscribe();
    }

    virtual void Terminate() {
        OpmMessageHook::Stop();
        OpmTriggerState::Reset();

        if (m_bridge != nullptr) {
            delete m_bridge;
            m_bridge = nullptr;
        }
    }
};
