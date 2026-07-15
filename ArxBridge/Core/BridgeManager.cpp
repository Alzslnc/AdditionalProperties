#include "BridgeManager.h"
#include <msclr/marshal_cppstd.h>

#include "../Model/SelectedDataClass.h"
#include "../Model/SelectedObjectData.h"
#include "../Core/ArxBridgeCore.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace Autodesk::AutoCAD::DatabaseServices;

namespace ArxBridge {

    void BridgeManager::Initialize() {
        if (m_providers == nullptr) {       
            m_providers = gcnew List<SelectedDataClass^>();
        }
    }

    void BridgeManager::RegisterProvider(SelectedDataClass^ provider) {
        if (provider != nullptr && m_providers != nullptr && !m_providers->Contains(provider)) {
            m_providers->Add(provider);

            // Перебираем новые объекты метаданных PropertyMeta
            for each (KeyValuePair<System::String^, PropertyMeta^> ^ pair in provider->AllProperties)
            {
                PropertyMeta^ meta = pair->Value;

                // Проверяем флаг включения свойства (актуально для вашего будущего окна выбора)
                if (meta != nullptr && meta->IsEnabled == true)
                {
                    // 🚀 МОЩНЫЙ СИНХРОННЫЙ ВЫЗОВ: Передаем имя, уникальное описание и тип из C# в COM!
                    ArxBridgeCore::RegisterDynamicProperty(meta->Name, meta->Description, meta->PropType);
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

                Entity^ entity = dynamic_cast<Entity^>(tr->GetObject(id, OpenMode::ForRead, false, true));
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
        }
    }

    void BridgeManager::ExecuteBatchWrite(List<SelectedObjectData^>^ changedData) {
        if (changedData == nullptr || changedData->Count == 0 || m_providers == nullptr) {
            return;
        }

        Database^ db = HostApplicationServices::WorkingDatabase;
        if (db == nullptr) return;

        Transaction^ tr = db->TransactionManager->StartTransaction();
        try {
            for (int i = 0; i < changedData->Count; ++i) {
                SelectedObjectData^ objectData = changedData[i];             
                             
                if (objectData == nullptr || !objectData->IsChanged) continue;

                ObjectId id = objectData->ObjectId;
                if (!id.IsValid) continue;

                // 2. 🚀 ВАЖНО: Открываем объект из базы данных AutoCAD строго НА ЗАПИСЬ (ForWrite)
                Entity^ entity = dynamic_cast<Entity^>(tr->GetObject(id, OpenMode::ForWrite, false, true));
                if (entity == nullptr) continue;

                // 3. Передаем объект C# провайдерам для применения геометрии
                for each(SelectedDataClass ^ provider in m_providers) {
                    try {
                        provider->SetProperties(entity, objectData->Properties);
                    }
                    catch (...) {}
                }
            }
            // Фиксируем пакет изменений на чертеже
            tr->Commit();
        }
        catch (...) {
            // В случае сбоя транзакция автоматически сбросится (Abort)
        }
    }


}
