#include "CacheInvalidationSubscription.h"
#include "DocumentCacheManager.h" 
#include <dbmain.h>
#include <acdocman.h>

namespace ArxBridge {

    CacheInvalidationSubscription& CacheInvalidationSubscription::getInstance() {
        static CacheInvalidationSubscription instance;
        return instance;
    }

    void CacheInvalidationSubscription::OnDocumentToBeDestroyed(AcApDocument* pDoc) {
        if (pDoc && pDoc->database()) {
            // КРИТИЧЕСКИ ВАЖНО: Уничтожаем пакетный кэш закрываемого чертежа для защиты от утечек ОЗУ
            DocumentCacheManager::ClearCache(pDoc->database());
        }
    }

    void CacheInvalidationSubscription::OnObjectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) {
        if (pDb && pObj) {
            AcDbDatabase* pModDb = const_cast<AcDbDatabase*>(pDb);

            // СЕРДЦЕ ЛЕНИВОЙ АКТУАЛИЗАЦИИ (Ваш оригинальный проверенный код): 
            // Точечно взводим флаг IsModified = true для этого объекта в пакетном кэше конкретного чертежа.
            DocumentCacheManager::InvalidateObject(pModDb, pObj->objectId());
        }
    }

} // namespace ArxBridge
