#include "PropertyReactor.h"
#include "PropertyEngine.h" 
#include "DocumentCacheManager.h"

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
            acDocManager->addReactor(this);
            UpdateAllDatabases(this, true);
        }
    }

    void PropertyReactor::stopMonitoring() {
        if (acDocManager != nullptr) {
            acDocManager->removeReactor(this);
            UpdateAllDatabases(this, false);
        }
    }

    void PropertyReactor::documentCreated(AcApDocument* pDoc) {
        if (pDoc && pDoc->database()) {
            pDoc->database()->addReactor(this);
        }
    }

    void PropertyReactor::documentToBeDestroyed(AcApDocument* pDoc) {
        if (pDoc && pDoc->database()) {
            pDoc->database()->removeReactor(this);
            // КРИТИЧЕСКИ ВАЖНО: Уничтожаем пакетный кэш закрываемого чертежа для защиты от утечек ОЗУ
            DocumentCacheManager::ClearCache(pDoc->database());
        }
    }

    void PropertyReactor::objectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) {
        if (pDb && pObj) {
            AcDbDatabase* pModDb = const_cast<AcDbDatabase*>(pDb);

            // СЕРДЦЕ ЛЕНИВОЙ АКТУАЛИЗАЦИИ: 
            // Точечно взводим флаг IsModified = true для этого объекта в пакетном кэше конкретного чертежа.
            DocumentCacheManager::InvalidateObject(pModDb, pObj->objectId());
        }
    }
} // namespace ArxBridge
