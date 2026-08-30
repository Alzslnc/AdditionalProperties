#include "OpmPaletteSubscription.h"
#include "BridgePropertyWatcher.h"
#include "PropertyEngine.h"
#include <dbid.h> // На всякий случай для AcDbObjectId
#include "OpmTriggerState.h"

namespace ArxBridge {

    namespace ArxBridgeCore {
        extern const std::vector<::AcDbObjectId>* m_frameSelectedIds;
    }

    OpmPaletteSubscription& OpmPaletteSubscription::getInstance() {
        static OpmPaletteSubscription instance;
        return instance;
    }

    // Хранилище адреса функции
    static void (*g_comboCallback)() = nullptr;

    void OpmPaletteSubscription::RegisterComboBoxCallback(void (*callback)())
    {
        g_comboCallback = callback;
    }

    OpmPaletteSubscription::OpmPaletteSubscription() : m_needsRefresh(false) {}

    void OpmPaletteSubscription::OnObjectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) {
        if (!BridgePropertyWatcher::getInstance().IsInternalUpdate() && !BridgePropertyWatcher::getInstance().IsEmpty()) {
            m_needsRefresh = true;
        }
    }

    // ИСПРАВЛЕНО: Принимаем указатель на вектор и безусловно обновляем кэш
    void OpmPaletteSubscription::NotifyModelessOperationEnded(const std::vector<::AcDbObjectId>* pSelectedIds) {
        if (m_needsRefresh) {

            // Если список Id передан и он не пустой — запускаем ваш пакетный перерасчет кэша
            if (pSelectedIds != nullptr) {
                PropertyEngine::PrepareInitialSelectionContext(*pSelectedIds);
            }

            // Направленный пинок палитры свойств
            BridgePropertyWatcher::getInstance().Refresh();

            m_needsRefresh = false;
        }
    }

    void OpmPaletteSubscription::OnDocumentToBeDestroyed(AcApDocument* pDoc) {
        BridgePropertyWatcher::getInstance().Clear();
        m_needsRefresh = false;
    }

    void OpmPaletteSubscription::OnDocumentLockModeChanged(const ACHAR* pGlobalCmdName)
    {
        // Пропускаем через нативный Win32 таймер-сито
        OpmTriggerState::ProcessLockMode(pGlobalCmdName);
    }

} // namespace ArxBridge
