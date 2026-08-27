#pragma once
#include "PropertyReactor.h"

namespace ArxBridge {

    /// <summary>
    /// Специализированный синглтон-подписчик. 
    /// Отвечает строго за инвалидацию и очистку пакетного кэша объектов баз данных чертежей.
    /// </summary>
    class CacheInvalidationSubscription : public BridgeSubscriptionBase {
    private:
        CacheInvalidationSubscription() = default;
        ~CacheInvalidationSubscription() override = default;

        CacheInvalidationSubscription(const CacheInvalidationSubscription&) = delete;
        CacheInvalidationSubscription& operator=(const CacheInvalidationSubscription&) = delete;

    public:        
        /// <summary> Получить доступ к единственному экземпляру подписки кэша. </summary>
        static CacheInvalidationSubscription& getInstance();

        // Реализация методов интерфейса BridgeSubscriptionBase
        void OnDocumentToBeDestroyed(AcApDocument* pDoc) override;
        void OnObjectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) override;
    };

} // namespace ArxBridge
