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
    void BridgeManager::Terminate() {      
        if (m_providers != nullptr) {
            m_providers->Clear();
            m_providers = nullptr; // Возвращаем в исходный nullptr для следующего цикла
        }
    }
    void BridgeManager::RegisterProvider(System::Object^ providerObj) {
        if (providerObj == nullptr || m_providers == nullptr) return;

        // Безопасное приведение типов внутри С++
        SelectedDataClass^ provider = dynamic_cast<SelectedDataClass^>(providerObj);
        if (provider == nullptr || provider->AllProperties == nullptr) return;

        if (!m_providers->Contains(provider)) {
            m_providers->Add(provider);

            // Читаем коллекцию ключей метаданных, которую нам передал C# модуль
            System::Collections::Generic::Dictionary<System::String^, PropertyMeta^>::KeyCollection^ keys = provider->AllProperties->Keys;

            for each (System::String ^ key in keys)
            {
                PropertyMeta^ meta = provider->AllProperties[key];

                if (meta != nullptr && meta->IsEnabled)
                {
                    // 🚀 Динамическая регистрация! В контексте открытого документа 
                    // AutoCAD примет её идеально и без вылетов RPC!
                    ArxBridgeCore::RegisterDynamicProperty(meta->Name, meta->Description, meta->PropType);
                }
            }
        }
    }

    void BridgeManager::RegisterProviderTest() {
        // Поставьте точку останова СЮДА!
        ArxBridgeCore::RegisterDynamicProperty(L"Тест", L"Описание", PropType::IsDouble);
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
