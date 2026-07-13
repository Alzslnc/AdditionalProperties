#include "BridgeManager.h"

// 1. Сначала принудительно подключаем стандартные контейнеры C++ 
// (Это даст файлу dbid.h понимание, что такое std::hash)
#include <map>
#include <functional>
#include <vcclr.h>

#include "../Model/SelectedDataClass.h"
#include "../Model/SelectedObjectData.h"
#include "../Model/Property.h"


using namespace System;
using namespace System::Collections::Generic;
using namespace Autodesk::AutoCAD::DatabaseServices;

gcroot<List<ArxBridge::SelectedDataClass^>^> BridgeManager::m_providers;

void BridgeManager::Initialize() {
    if (((List<ArxBridge::SelectedDataClass^>^)m_providers) == nullptr) {
        m_providers = gcnew List<ArxBridge::SelectedDataClass^>();
    }
}

void BridgeManager::RegisterProvider(ArxBridge::SelectedDataClass^ provider) {
    if (provider != nullptr && ((List<ArxBridge::SelectedDataClass^>^)m_providers) != nullptr && !m_providers->Contains(provider)) {
        m_providers->Add(provider);
    }
}

/// <summary>
/// ПАКЕТНЫЙ ОБНОВИТЕЛЬ ПАРАМЕТРОВ КЭША (Максимально облегченный вариант)
/// </summary>
void BridgeManager::ExecuteBatchUpdate(List<ArxBridge::SelectedObjectData^>^ nonActualData) {

    if (nonActualData == nullptr || nonActualData->Count == 0 || ((List<ArxBridge::SelectedDataClass^>^)m_providers) == nullptr || m_providers->Count == 0) {
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
            if (!id.IsValid) continue;

            Entity^ entity = dynamic_cast<Entity^>(tr->GetObject(id, OpenMode::ForRead));
            if (entity == nullptr) continue;

            // МЫ ПОЛНОСТЬЮ УБРАЛИ ОТСЮДА ОЧИСТКУ И ИНИЦИАЛИЗАЦИЮ СПИСКА!
            // Объект приходит уже подготовленным из PropertyEngine/PropertyReactor.

            objectData->ClassName = entity->GetType()->Name;
            objectData->IsModified = false; // Сбрасываем флаг, так как сейчас данные обновятся

            // Извлекаем список провайдеров из gcroot для безопасного for each
            List<ArxBridge::SelectedDataClass^>^ currentProviders = m_providers;

            for each (ArxBridge::SelectedDataClass ^ provider in currentProviders) {
                try {
                    provider->GetProperties(entity, objectData); // C# наследник просто добавляет свойства в Properties
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
