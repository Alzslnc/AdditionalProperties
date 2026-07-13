#include "PropertyReactor.h"
#include "PropertyEngine.h" 

namespace ArxBridge {

    PropertyReactor::PropertyReactor() {}
    PropertyReactor::~PropertyReactor() { stopMonitoring(); }

    PropertyReactor& PropertyReactor::getInstance() {
        static PropertyReactor instance;
        return instance;
    }

    void PropertyReactor::startMonitoring() {
        if (acDocManager != nullptr) {
            acDocManager->addReactor(this);

            // Накатываем реактор на все уже открытые чертежи при старте плагина
            // Используем строго новый синтаксис newAcApDocumentIterator() [INDEX]
            AcApDocumentIterator* pIt = acDocManager->newAcApDocumentIterator();
            if (pIt != nullptr) {
                for (; !pIt->done(); pIt->step()) {
                    AcApDocument* pDoc = pIt->document();
                    if (pDoc && pDoc->database()) {
                        pDoc->database()->addReactor(this);
                    }
                }
                delete pIt;
            }
        }
    }

    void PropertyReactor::stopMonitoring() {
        if (acDocManager != nullptr) {
            acDocManager->removeReactor(this);

            AcApDocumentIterator* pIt = acDocManager->newAcApDocumentIterator();
            if (pIt != nullptr) {
                for (; !pIt->done(); pIt->step()) {
                    AcApDocument* pDoc = pIt->document();
                    if (pDoc && pDoc->database()) {
                        pDoc->database()->removeReactor(this);
                    }
                }
                delete pIt;
            }
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
            PropertyEngine::ClearCache(pDoc->database()); // Защита от утечек ОЗУ
        }
    }

    void PropertyReactor::documentActivated(AcApDocument* pDoc) {
        PropertyEngine::AggregatedDoubles.clear();
        PropertyEngine::AggregatedInts.clear();
    }

    void PropertyReactor::objectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) {
        if (pDb && pObj) {
            AcDbDatabase* pModDb = const_cast<AcDbDatabase*>(pDb);
            PropertyEngine::InvalidateObject(pModDb, pObj->objectId()); // Мгновенный точечный сброс флага
        }
    }
}
