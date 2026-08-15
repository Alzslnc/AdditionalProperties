#pragma once
#include <vector>

namespace ArxBridge {

    struct ActivePropertyPair;

    /// <summary>
    /// Чистый нативный контейнер данных (Data Holder). 
    /// Хранит активные контексты кастомных свойств палитры OPM.
    /// </summary>
    class BridgePropertyWatcher {
    private:
        std::vector<ActivePropertyPair>* m_pActiveProps;
        bool m_isInternalUpdate;

        BridgePropertyWatcher();
        ~BridgePropertyWatcher();
        BridgePropertyWatcher(const BridgePropertyWatcher&) = delete;
        BridgePropertyWatcher& operator=(const BridgePropertyWatcher&) = delete;

    public:
        static BridgePropertyWatcher& getInstance();

        // ТОЧНЫЕ ИМЕНА ИЗ ВАШЕГО ПРОЕКТА ДЛЯ CONNECT / DISCONNECT
        void RegisterActiveContext(void* pPropertyIdentity, void* pNotifySink2);
        void UnregisterActiveContext(void* pPropertyIdentity);

        void SetInternalUpdate(bool isInternal);
        bool IsInternalUpdate() const;
        bool IsEmpty() const;

        void Refresh();
        void Clear();
    };

} // namespace ArxBridge
