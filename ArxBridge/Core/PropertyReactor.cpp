#include "PropertyReactor.h"
#include "PropertyEngine.h" 

namespace ArxBridge {

    PropertyReactor::PropertyReactor() {}
    PropertyReactor::~PropertyReactor() {
        // При деструкции глобального статического объекта acDocManager может быть уже nullptr.
        // Передаем проверку внутрь.
        stopMonitoring();
    }

    PropertyReactor& PropertyReactor::getInstance() {
        static PropertyReactor instance;
        return instance;
    }

    // Вспомогательный приватный метод для массовой подписки/отписки от баз данных dwg
    // (Этого метода нет в .h, его можно вызвать локально, если объявить как private или прямо тут)
    void UpdateAllDatabases(PropertyReactor* pReactor, bool attach) {
        if (acDocManager == nullptr) return;

        AcApDocumentIterator* pIt = acDocManager->newAcApDocumentIterator();
        if (pIt != nullptr) {
            for (; !pIt->done(); pIt->step()) {
                AcApDocument* pDoc = pIt->document();
                if (pDoc && pDoc->database()) {
                    if (attach) {
                        pDoc->database()->addReactor(pReactor); // Теперь компилятор счастлив!
                    }
                    else {
                        pDoc->database()->removeReactor(pReactor);
                    }
                }
            }
            delete pIt;
        }
    }

    void PropertyReactor::startMonitoring() {
        if (acDocManager != nullptr) {
            acDocManager->addReactor(this);
            UpdateAllDatabases(this, true); // Подключаем реакторы на лету
        }
    }

    void PropertyReactor::stopMonitoring() {
        if (acDocManager != nullptr) {
            acDocManager->removeReactor(this);
            UpdateAllDatabases(this, false); // Чисто отписываемся от всех окон
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
            // const_cast необходим, так как кэш PropertyEngine принимает не-const указатель
            AcDbDatabase* pModDb = const_cast<AcDbDatabase*>(pDb);
            PropertyEngine::InvalidateObject(pModDb, pObj->objectId()); // Мгновенный точечный сброс флага
        }
    }
} // namespace ArxBridge
