#pragma once
#include "PropertyReactor.h"
#include <vector> // Добавили инклюд векторов

class AcDbObjectId; // Опережающее объявление

namespace ArxBridge {

    class OpmPaletteSubscription : public BridgeSubscriptionBase {
    private:
        bool m_needsRefresh;

        OpmPaletteSubscription();
        ~OpmPaletteSubscription() override = default;

        OpmPaletteSubscription(const OpmPaletteSubscription&) = delete;
        OpmPaletteSubscription& operator=(const OpmPaletteSubscription&) = delete;

    public:   
        static OpmPaletteSubscription& getInstance();

        static void RegisterComboBoxCallback(void (*callback)());

        // ИСПРАВЛЕНО: Теперь метод принимает нативный указатель на вектор ID
        void NotifyModelessOperationEnded(const std::vector<::AcDbObjectId>* pSelectedIds);

        void OnObjectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) override;
        void OnDocumentToBeDestroyed(AcApDocument* pDoc) override;

   
        void OnDocumentLockModeChanged(const ACHAR* pGlobalCmdName) override;

    };

} // namespace ArxBridge
