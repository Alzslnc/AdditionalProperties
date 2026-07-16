#include "BridgeManager.h"
#include <msclr/marshal_cppstd.h>

#include "../Model/SelectedDataClass.h"
#include "../Model/SelectedObjectData.h"
#include "../Core/ArxBridgeCore.h"

using namespace System;
using namespace Autodesk::AutoCAD::DatabaseServices;
using namespace System::Collections::Generic;

namespace ArxBridge {

    void BridgeManager::Initialize() {
        if (m_providers == nullptr) {
            m_providers = gcnew List<SelectedDataClass^>();
        }
    }

    void BridgeManager::Terminate() {
        if (m_providers != nullptr) {
            m_providers->Clear();
            m_providers = nullptr;
        }
    }

    void BridgeManager::RegisterProvider(System::Object^ providerObj) {
        if (providerObj == nullptr || m_providers == nullptr) return;

        SelectedDataClass^ provider = dynamic_cast<SelectedDataClass^>(providerObj);
        if (provider == nullptr || provider->AllProperties == nullptr) return;

        if (!m_providers->Contains(provider)) {
            m_providers->Add(provider);

            Dictionary<System::String^, PropertyMeta^>::KeyCollection^ keys = provider->AllProperties->Keys;

            for each (System::String ^ key in keys)
            {
                PropertyMeta^ meta = provider->AllProperties[key];

                if (meta != nullptr && meta->IsEnabled)
                {
                    List<System::String^>^ nativeClassNames = gcnew List<System::String^>();

                    for each (System::Type ^ managedType in meta->Types)
                    {
                        if (managedType == nullptr) continue;

                        try {
                            Autodesk::AutoCAD::Runtime::RXClass^ rxClass =
                                Autodesk::AutoCAD::Runtime::RXClass::GetClass(managedType);

                            if (rxClass != nullptr && !System::String::IsNullOrEmpty(rxClass->Name))
                            {
                                nativeClassNames->Add(rxClass->Name);
                            }
                        }
                        catch (System::Exception^ ex) {
                            System::Diagnostics::Debug::WriteLine("Ошибка маршалинга RXClass: " + ex->Message);
                        }
                    }

                    if (nativeClassNames->Count > 0) {
                        ArxBridgeCore::RegisterDynamicPropertyForClasses(
                            meta->Name,
                            meta->Description,
                            meta->PropType,
                            nativeClassNames
                        );
                    }
                }
            }
        }
    }

    // 🚀 ЕДИНЫЙ ВЫСОКОСКОРОСТНОЙ КОНВЕЙЕР СИНХРОНИЗАЦИИ ДАННЫХ
    // (Покрывает и Фазу Чтения, и Фазу Записи на месте без дублирования)
    void BridgeManager::ExecuteBatchSync(List<SelectedObjectData^>^ objectsData, bool isWriteMode) {
        if (objectsData == nullptr || objectsData->Count == 0 || m_providers == nullptr) {
            return;
        }

        Autodesk::AutoCAD::ApplicationServices::Document^ doc =
            Autodesk::AutoCAD::ApplicationServices::Application::DocumentManager->MdiActiveDocument;
        if (doc == nullptr) return;

        // 🚀 ИСПРАВЛЕНО: Явный указатель с крышечкой для безопасного деструктурирования
        Autodesk::AutoCAD::ApplicationServices::DocumentLock^ docLock = doc->LockDocument();

        try {
            Database^ db = doc->Database;
            if (db == nullptr) return;

            Transaction^ tr = db->TransactionManager->StartTransaction();
            try {
                for (int i = 0; i < objectsData->Count; ++i) {
                    SelectedObjectData^ objectData = objectsData[i];
                    if (objectData == nullptr) continue;

                    ObjectId id = objectData->ObjectId;
                    if (!id.IsValid) continue;

                    // Если идет фиксация изменений — запрашиваем ForWrite, иначе — ForRead
                    OpenMode mode = isWriteMode ? OpenMode::ForWrite : OpenMode::ForRead;

                    Entity^ entity = dynamic_cast<Entity^>(tr->GetObject(id, mode, false, true));
                    if (entity == nullptr) continue;

                    // 📥 ТАКТ 1. ЗАПИСЬ (Срабатывает только в режиме фиксации изменений)
                    if (isWriteMode && objectData->IsChanged) {
                        for each (SelectedDataClass ^ provider in m_providers) {
                            if (provider != nullptr) {
                                try { provider->SetProperties(entity, objectData->Properties); }
                                catch (...) {}
                            }
                        }
                    }

                    // Системный паспорт типа объекта (всегда актуализируем при проходе)
                    objectData->ClassName = entity->GetType()->Name;
                    objectData->IsModified = false;

                    // 📤 ТАКТ 2. ЧТЕНИЕ ПО ГОРЯЧИМ СЛЕДАМ (Перечитываем честную точность геометрии из БД)
                    for each (SelectedDataClass ^ provider in m_providers) {
                        if (provider != nullptr) {
                            try 
                            { 
                                provider->PreAcadUpdateAction();
                                provider->GetProperties(entity, objectData); 
                            }
                            catch (...) {}
                        }
                    }

                    // 🧹 ТАКТ 3. СБРОС ФЛАГОВ ИЗМЕНЕНИЙ (После успешного перечитывания зануляем NewValue)
                    if (isWriteMode && objectData->Properties != nullptr) {
                        for (int j = 0; j < objectData->Properties->Count; ++j) {
                            if (objectData->Properties[j] != nullptr) {
                                objectData->Properties[j]->NewValue = nullptr; // IsChanged автоматически станет false
                            }
                        }
                    }
                }
                tr->Commit();
            }
            catch (System::Exception^ ex) {
                System::Diagnostics::Debug::WriteLine("Сбой конвейера синхронизации OPM: " + ex->Message);
                tr->Abort();
            }
            finally {
                delete tr; // Уничтожаем транзакцию CLR
            }
        }
        finally {
            // 🚀 БЕЗОПАСНЫЙ СБРОС БЛОКИРОВКИ ДОКУМЕНТА
            if (docLock != nullptr) {
                delete docLock;
            }
        }
    }
}
