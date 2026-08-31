#include "BridgePropertyWatcher.h"
#include <unknwn.h>
#include "ArxBridgeCore.h"
#include "PropertyEngine.h"

// Локальная копия интерфейса AutoCAD из SDK для бинарного vtable-matching
interface IDynamicPropertyNotify2_Local : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE OnChanged(IUnknown* pDynamicProperty) = 0;
};

namespace ArxBridge {

    struct ActivePropertyPair {
        void* pPropertyIdentity;
        IDynamicPropertyNotify2_Local* pSinkNotify;
    };

    BridgePropertyWatcher::BridgePropertyWatcher() : m_isInternalUpdate(false) {
        m_pActiveProps = new std::vector<ActivePropertyPair>();
    }

    BridgePropertyWatcher::~BridgePropertyWatcher() {
        Clear();
        delete m_pActiveProps;
    }

    BridgePropertyWatcher& BridgePropertyWatcher::getInstance() {
        static BridgePropertyWatcher instance;
        return instance;
    }

    // ИСПРАВЛЕНО: НАЗВАНИЕ МЕТОДА ВОЗВРАЩЕНО НА REGISTERACTIVECONTEXT
    void BridgePropertyWatcher::RegisterActiveContext(void* pPropertyIdentity, void* pNotifySink2) {
        if (!pPropertyIdentity || !pNotifySink2) return;
        IDynamicPropertyNotify2_Local* pSink = static_cast<IDynamicPropertyNotify2_Local*>(pNotifySink2);

        for (const auto& pair : *m_pActiveProps) {
            if (pair.pPropertyIdentity == pPropertyIdentity) return;
        }

        pSink->AddRef(); // Увеличиваем счетчик COM-ссылок палитры
        m_pActiveProps->push_back({ pPropertyIdentity, pSink });
    }

    // ИСПРАВЛЕНО: НАЗВАНИЕ МЕТОДА ВОЗВРАЩЕНО НА UNREGISTERACTIVECONTEXT
    void BridgePropertyWatcher::UnregisterActiveContext(void* pPropertyIdentity) {
        for (auto it = m_pActiveProps->begin(); it != m_pActiveProps->end(); ++it) {
            if (it->pPropertyIdentity == pPropertyIdentity) {
                if (it->pSinkNotify) it->pSinkNotify->Release();
                m_pActiveProps->erase(it);
                break;
            }
        }
    }

    void BridgePropertyWatcher::SetInternalUpdate(bool isInternal) { m_isInternalUpdate = isInternal; }
    bool BridgePropertyWatcher::IsInternalUpdate() const { return m_isInternalUpdate; }
    bool BridgePropertyWatcher::IsEmpty() const { return m_pActiveProps->empty(); }

    void BridgePropertyWatcher::Clear() {
        for (auto& pair : *m_pActiveProps) {
            if (pair.pSinkNotify) pair.pSinkNotify->Release();
        }
        m_pActiveProps->clear();
    }

    void BridgePropertyWatcher::Refresh() {
        if (m_isInternalUpdate || m_pActiveProps->empty()) return;
                
        // 🚀 БЕЗОПАСНАЯ АКТУАЛИЗАЦИЯ КЭША:
        const std::vector<::AcDbObjectId>* pNativeIds = ArxBridge::ArxBridgeCore::m_frameSelectedIds;
        if (pNativeIds != nullptr && !pNativeIds->empty()) {
            ArxBridge::PropertyEngine::PrepareInitialSelectionContext(*pNativeIds);
        }

        for (const auto& pair : *m_pActiveProps) {
            if (pair.pSinkNotify && pair.pPropertyIdentity) {
                IUnknown* pPropUnk = static_cast<IUnknown*>(pair.pPropertyIdentity);
                pair.pSinkNotify->OnChanged(pPropUnk);
            }
        }
    }

} // namespace ArxBridge
