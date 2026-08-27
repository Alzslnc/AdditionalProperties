#include "OpmPaletteSubscription.h"
#include "BridgePropertyWatcher.h"
#include "PropertyEngine.h"
#include <dbid.h> // На всякий случай для AcDbObjectId


namespace ArxBridge {

    OpmPaletteSubscription& OpmPaletteSubscription::getInstance() {
        static OpmPaletteSubscription instance;
        return instance;
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
        // Подписка палитры сама решает, какая именно UI-команда её интересует
        if (pGlobalCmdName != nullptr && (std::wcsstr(pGlobalCmdName, L"PREVIEW_END") != nullptr || std::wcsstr(pGlobalCmdName, L"OpmPropertyValueUpdate") != nullptr))
        {
            // Взводим флаг внутри этого же класса (или синглтона)
            this->m_isCacheDirty = true;
        }
    }

} // namespace ArxBridge
