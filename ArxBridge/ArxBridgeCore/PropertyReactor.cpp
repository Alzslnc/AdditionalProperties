#include "PropertyReactor.h"
#include "CacheInvalidationSubscription.h" // Подписка сброса кэша БД
#include "OpmPaletteSubscription.h"        // Подписка отслеживания изменений палитры свойств

namespace {
    /// <summary>
    /// Вспомогательная функция для массовой подписки или отписки реактора от баз данных всех открытых чертежей.
    /// </summary>
    void UpdateAllDatabases(ArxBridge::PropertyReactor* pReactor, bool attach) {
        if (acDocManager == nullptr) return;

        AcApDocumentIterator* pIt = acDocManager->newAcApDocumentIterator();
        if (pIt != nullptr) {
            for (; !pIt->done(); pIt->step()) {
                AcApDocument* pDoc = pIt->document();
                if (pDoc && pDoc->database()) {
                    if (attach) {
                        pDoc->database()->addReactor(pReactor);
                    }
                    else {
                        pDoc->database()->removeReactor(pReactor);
                    }
                }
            }
            delete pIt;
        }
    }
} // namespace

namespace ArxBridge {

    PropertyReactor::PropertyReactor() {}

    PropertyReactor::~PropertyReactor() {
        stopMonitoring();
    }

    PropertyReactor& PropertyReactor::getInstance() {
        static PropertyReactor instance;
        return instance;
    }

    void PropertyReactor::startMonitoring() {
        if (acDocManager != nullptr) {
            // Гарантированно очищаем вектор перед наполнением
            m_subscriptions.clear();

            // САМОСТОЯТЕЛЬНО НАПОЛНЯЕМ СЕРВИС НАТИВНЫМИ ПОДПИСКАМИ ПРИ СТАРТЕ
            // 1. Первая подписка: ваш вынесенный код сброса кэша объектов в БД [^5]
            m_subscriptions.push_back(&CacheInvalidationSubscription::getInstance());

            // 2. Вторая подписка: нативный координатор тактов для палитры OPM
            m_subscriptions.push_back(&OpmPaletteSubscription::getInstance());

            // Регистрируем нативное ядро в менеджере документов AutoCAD
            acDocManager->addReactor(this);
            UpdateAllDatabases(this, true);
        }
    }

    void PropertyReactor::stopMonitoring() {
        if (acDocManager != nullptr) {
            acDocManager->removeReactor(this);
            UpdateAllDatabases(this, false);

            // Очищаем вектор подписок при остановке, разрывая связи
            m_subscriptions.clear();
        }
    }

    void PropertyReactor::documentCreated(AcApDocument* pDoc) {
        if (pDoc && pDoc->database()) {
            pDoc->database()->addReactor(this);

            // Транслируем событие во все зарегистрированные нативные подписки
            for (auto* sub : m_subscriptions) {
                if (sub) sub->OnDocumentCreated(pDoc);
            }
        }
    }

    void PropertyReactor::documentToBeDestroyed(AcApDocument* pDoc) {
        if (pDoc && pDoc->database()) {
            pDoc->database()->removeReactor(this);
        }

        // Транслируем событие уничтожения документа во все подписки (для очистки контекстов и кэшей чертежа)
        for (auto* sub : m_subscriptions) {
            if (sub) sub->OnDocumentToBeDestroyed(pDoc);
        }
    }

    void PropertyReactor::objectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) {
        // Транслируем нативное событие модификации объекта базы данных во все подписки пакета [^5].
        // CacheInvalidationSubscription взведет сброс кэша, а OpmPaletteSubscription — зафиксирует изменение для UI.
        for (auto* sub : m_subscriptions) {
            if (sub) sub->OnObjectModified(pDb, pObj);
        }
    }

    void PropertyReactor::documentLockModeChanged(
        AcApDocument* pDoc,
        AcAp::DocLockMode myPreviousMode,
        AcAp::DocLockMode myCurrentMode,
        AcAp::DocLockMode currentMode,
        const ACHAR* pGlobalCmdName)
    {
        for (auto* subscription : m_subscriptions)
        {
            if (subscription != nullptr)
            {
                subscription->OnDocumentLockModeChanged(pGlobalCmdName);
            }
        }
    }
} // namespace ArxBridge
