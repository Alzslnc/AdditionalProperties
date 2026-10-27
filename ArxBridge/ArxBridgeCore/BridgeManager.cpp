#include "BridgeManager.h"
#include <msclr/marshal_cppstd.h>
#include "SelectedDataClass.h"
#include "PropertyConfigurator.h"
#include "SelectedObjectData.h"
#include "Property.h" 
#include "ArxBridgeCore.h"
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

        // 🚀 ШАГ 1: Актуализируем флаги IsEnabled и заполняем данные для WPF ДО прохода цикла.
        // Метод внутри себя определит имя сборки, проверит XML и обновит метаданные провайдера.
        PropertyConfigurator::UpdatePropertiesFromXml(provider, provider->AllProperties);

        if (!m_providers->Contains(provider)) {
            m_providers->Add(provider);

            Dictionary<System::String^, PropertyMeta^>::KeyCollection^ keys = provider->AllProperties->Keys;

            for each (System::String ^ key in keys)
            {
                PropertyMeta^ meta = provider->AllProperties[key];

                // 🚀 ШАГ 2: Сюда гарантированно приходит уже актуальный флаг IsEnabled из XML-конфига!
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
                            meta,
                            nativeClassNames
                        );
                    }
                }
            }
        }
    }

    void BridgeManager::UpdateProviders() {
        if (m_providers == nullptr || m_providers->Count == 0) return;

        // Кэш для сборки итогового XML (Key: UniqueId, Value: IsEnabled)
        // Он нужен, чтобы за один проход сохранить настройки всех плагинов на диск
        Dictionary<System::String^, bool>^ allSettingsToSave = gcnew Dictionary<System::String^, bool>();

        for each (SelectedDataClass ^ provider in m_providers) {
            if (provider == nullptr || provider->AllProperties == nullptr) continue;

            for each (PropertyMeta ^ meta in provider->AllProperties->Values) {
                if (meta == nullptr || meta->AssemblyName == nullptr || meta->ProviderClassName == nullptr) continue;

                // Собираем уникальный ID для этого свойства
                System::String^ xmlLookupId = meta->AssemblyName + "|" + meta->ProviderClassName + "|" + meta->Name;

                // Добавляем текущее состояние флага (которое пользователь накликал в WPF) в общую карту для сохранения
                allSettingsToSave[xmlLookupId] = meta->IsEnabled;

                // 💡 Здесь можно оставить вашу нативную проверку: 
                // если свойство выключено в UI, но всё еще зарегистрировано в AutoCAD (или наоборот) — 
                // вызываем ArxBridgeCore::UnregisterDynamicProperty / RegisterDynamicPropertyForClasses.
                // (Для этого можно в метаданные добавить внутренний флаг bool m_isCurrentlyRegisteredInAcad)
            }
        }

        // 🚀 ОДНА СТРОКА: Сбрасываем актуальное состояние из памяти в XML файл
        PropertyConfigurator::SaveAllPropertiesToXml(allSettingsToSave);
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
                                try { provider->SetProperties(entity, objectData->Properties, tr); }
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
                            try { provider->GetProperties(entity, objectData, tr); }
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
