#include "BridgeManager.h"

// 1. Сначала принудительно подключаем стандартные контейнеры C++ 
// (Это даст файлу dbid.h понимание, что такое std::hash)
#include <map>
#include <functional>
namespace std {}
#include <vcclr.h>

#include <msclr/marshal_cppstd.h>

#include "../Model/SelectedDataClass.h"
#include "../Model/SelectedObjectData.h"
#include "../Model/Property.h"
#include "../Core/ArxBridgeCore.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace Autodesk::AutoCAD::DatabaseServices;


void BridgeManager::Initialize() {
    // Теперь это обычная ссылка, проверяем ее БЕЗ приведений типов! [INDEX]
    if (m_providers == nullptr) {
        m_providers = gcnew List<ArxBridge::SelectedDataClass^>();
    }
}

namespace ArxBridge { ref class ArxBridgeCore; }

void BridgeManager::RegisterProvider(ArxBridge::SelectedDataClass^ provider) {
    if (provider != nullptr && m_providers != nullptr && !m_providers->Contains(provider)) {

        m_providers->Add(provider);

        // Перебираем свойства. Обратите внимание на стрелочки -> для ссылочных типов C++/CLI
        for each (KeyValuePair<System::String^, System::Boolean> ^ property in provider->AllProperties)
        {
            if (property->Value == true)
            {
                // 🚀 Теперь это работает идеально! Студия видит класс, а метод принимает property->Key напрямую!
                ArxBridge::ArxBridgeCore::RegisterDynamicProperty(property->Key);
            }
        }
    }
}

void BridgeManager::ExecuteBatchUpdate(List<ArxBridge::SelectedObjectData^>^ nonActualData) {
    if (nonActualData == nullptr || nonActualData->Count == 0 || m_providers == nullptr) {
        return;
    }

    Database^ db = HostApplicationServices::WorkingDatabase;
    if (db == nullptr) return;

    Transaction^ tr = db->TransactionManager->StartTransaction();
    try {
        for (int i = 0; i < nonActualData->Count; ++i) {
            ArxBridge::SelectedObjectData^ objectData = nonActualData[i];
            if (objectData == nullptr) continue;

            ObjectId id = objectData->ObjectId;
            if (!id.IsValid) continue; // Наш проверенный синтаксис с большой буквы!

            Entity^ entity = dynamic_cast<Entity^>(tr->GetObject(id, OpenMode::ForRead));
            if (entity == nullptr) continue;

            objectData->ClassName = entity->GetType()->Name;
            objectData->IsModified = false;

            // ИСПРАВЛЕНО: Никаких извлечений из gcroot больше не нужно! Перебираем напрямую [INDEX]
            for each (ArxBridge::SelectedDataClass ^ provider in m_providers) {
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

