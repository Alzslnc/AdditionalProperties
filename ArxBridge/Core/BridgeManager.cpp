
#include "BridgeManager.h"
#include <msclr/marshal_cppstd.h>

#include "../Model/SelectedDataClass.h"
#include "../Model/SelectedObjectData.h"
#include "../Core/ArxBridgeCore.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace Autodesk::AutoCAD::DatabaseServices;

// 🚀 Открываем пространство имен для реализации
namespace ArxBridge {

    void BridgeManager::Initialize() {
        if (m_providers == nullptr) {
            // Больше не нужно писать ArxBridge::SelectedDataClass^, мы уже внутри пространства!
            m_providers = gcnew List<SelectedDataClass^>();
        }
    }

    void BridgeManager::RegisterProvider(SelectedDataClass^ provider) {
        if (provider != nullptr && m_providers != nullptr && !m_providers->Contains(provider)) {
            m_providers->Add(provider);

            for each (KeyValuePair<System::String^, System::Boolean> ^ property in provider->AllProperties)
            {
                if (property->Value == true)
                {
                    // Убрали префикс ArxBridge:: перед ArxBridgeCore
                    ArxBridgeCore::RegisterDynamicProperty(property->Key);
                }
            }
        }
    }

    void BridgeManager::ExecuteBatchUpdate(List<SelectedObjectData^>^ nonActualData) {
        if (nonActualData == nullptr || nonActualData->Count == 0 || m_providers == nullptr) {
            return;
        }

        Database^ db = HostApplicationServices::WorkingDatabase;
        if (db == nullptr) return;

        Transaction^ tr = db->TransactionManager->StartTransaction();
        try {
            for (int i = 0; i < nonActualData->Count; ++i) {
                SelectedObjectData^ objectData = nonActualData[i];
                if (objectData == nullptr) continue;

                ObjectId id = objectData->ObjectId;
                if (!id.IsValid) continue;

                Entity^ entity = dynamic_cast<Entity^>(tr->GetObject(id, OpenMode::ForRead));
                if (entity == nullptr) continue;

                objectData->ClassName = entity->GetType()->Name;
                objectData->IsModified = false;

                for each (SelectedDataClass ^ provider in m_providers) {
                    try {
                        provider->GetProperties(entity, objectData);
                    }
                    catch (...) {}
                }
            }
            tr->Commit();
        }
        catch (...) {
            // Автоматический Abort
        }
    }

}
