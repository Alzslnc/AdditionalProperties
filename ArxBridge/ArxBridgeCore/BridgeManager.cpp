#include "BridgeManager.h"
#include <msclr/marshal_cppstd.h>

#include "SelectedDataClass.h"
#include "SelectedObjectData.h"
#include "Property.h" 
#include "ArxBridgeCore.h"
#include "OpmPaletteSubscription.h"
#include "OpmMessageHook.h"

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
    void BridgeManager::ExecuteBatchSync(List<SelectedObjectData^>^ objectsData, bool isWriteMode) {
        if (objectsData == nullptr || objectsData->Count == 0 || m_providers == nullptr) {
            return;
        }

        Autodesk::AutoCAD::ApplicationServices::Document^ doc =
            Autodesk::AutoCAD::ApplicationServices::Application::DocumentManager->MdiActiveDocument;
        if (doc == nullptr) return;

        // Явный указатель с крышечкой для безопасного деструктурирования контекста сессии AutoCAD
        Autodesk::AutoCAD::ApplicationServices::DocumentLock^ docLock = doc->LockDocument();

        try {
            Database^ db = doc->Database;
            if (db == nullptr) return;

            // 🚀 ОПТИМИЗАЦИЯ: Вызываем PreAcadUpdateAction ОДИН раз для каждого плагина 
            // перед стартом тяжелого цикла обхода объектов чертежа
            for each (SelectedDataClass ^ provider in m_providers) {
                if (provider != nullptr) {
                    try { provider->PreAcadUpdateAction(); }
                    catch (...) {}
                }
            }

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

                    // 📥 ТАКТ 1. ЗАПИСЬ И ФИКСАЦИЯ ИЗМЕНЕНИЙ В ЧЕРТЕЖ
                    if (isWriteMode && objectData->IsChanged) {
                        for each (SelectedDataClass ^ provider in m_providers) {
                            if (provider != nullptr) {
                                try { provider->SetProperties(entity, objectData->Properties); }
                                catch (...) {}
                            }
                        }
                    }

                    // Сбрасываем флаг модификации: объект теперь полностью актуален
                    // 🚀 ИСПРАВЛЕНО: Строка записи в удаленный ClassName полностью ликвидирована
                    objectData->IsModified = false;

                    // Сбрасываем старый набор, чтобы провайдеры заполнили кэш с чистого листа
                    if (objectData->Properties != nullptr) {
                        objectData->Properties->Clear(); // 🌟 Вызываем управляемый метод Clear()
                    }

                    // 📤 ТАКТ 2. МГНОВЕННОЕ ПЕРЕЧИТЫВАНИЕ (Сбор свежей геометрии в .NET кэш)
                    for each (SelectedDataClass ^ provider in m_providers) {
                        if (provider != nullptr) {
                            try { provider->GetProperties(entity, objectData); }
                            catch (...) {}
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
                delete tr; // Жестко уничтожаем транзакцию CLR
            }
        }
        finally {
            if (docLock != nullptr) {
                delete docLock; // Безопасный сброс блокировки dwg-файла
            }            
        }

        OpmMessageHook::IsChanged = false;
    }

    ArxBridge::PropertyMeta^ BridgeManager::FindMetadataAcrossProviders(System::String^ propName) {
        if (BridgeManager::m_providers != nullptr) {
            for each (ArxBridge::SelectedDataClass ^ provider in BridgeManager::m_providers) {
                if (provider != nullptr) {
                    ArxBridge::PropertyMeta^ meta = provider->GetPropertyMetadata(propName);
                    if (meta != nullptr) {
                        return meta;
                    }
                }
            }
        }
        return nullptr;
    }
}
