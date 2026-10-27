#include "PropertyConfigurator.h"
#include "Property.h"            // Подключаем для работы с типом PropertyMeta
#include "SelectedDataClass.h"   // Подключаем для работы с типом SelectedDataClass

using namespace System::IO;
using namespace System::Xml;
using namespace System::Reflection;

namespace ArxBridge {

    System::String^ PropertyConfigurator::GetXmlPath() {
        System::String^ appData = System::Environment::GetFolderPath(System::Environment::SpecialFolder::ApplicationData);
        // Универсальная общая папка для настроек вашего моста OPM
        System::String^ folder = Path::Combine(appData, "AlzAcadProgramSettings", "ArxBridge");

        if (!Directory::Exists(folder)) {
            Directory::CreateDirectory(folder);
        }
        return Path::Combine(folder, "PropertySettings.xml");
    }

    void PropertyConfigurator::UpdatePropertiesFromXml(System::Object^ providerObj, Dictionary<System::String^, PropertyMeta^>^ properties) {
        if (providerObj == nullptr || properties == nullptr || properties->Count == 0) return;

        // Безопасное приведение к базовому типу провайдера данных
        SelectedDataClass^ provider = dynamic_cast<SelectedDataClass^>(providerObj);
        if (provider == nullptr) return;

        // 🚀 РЕФЛЕКСИЯ: Извлекаем имя DLL (без расширения .dll) и системное имя класса провайдера
        System::String^ assemblyName = provider->GetType()->Assembly->GetName()->Name;
        System::String^ className = provider->GetType()->Name;
        System::String^ prefix = assemblyName + "|" + className + "|";

        System::String^ xmlPath = GetXmlPath();

        // Кэш для хранения объединенного состояния ВСЕХ свойств из XML (Ключ: ИмяDLL|Класс|Свойство, Значение: IsEnabled)
        Dictionary<System::String^, bool>^ xmlSettings = gcnew Dictionary<System::String^, bool>();

        // Если файл настроек уже существует на диске, считываем его текущее состояние
        if (File::Exists(xmlPath)) {
            try {
                XmlDocument^ doc = gcnew XmlDocument();
                doc->Load(xmlPath);
                XmlNodeList^ nodes = doc->SelectNodes("//Property");

                for each (XmlNode ^ node in nodes) {
                    if (node->Attributes["Id"] != nullptr && node->Attributes["IsEnabled"] != nullptr) {
                        System::String^ id = node->Attributes["Id"]->Value;
                        bool isEnabled = System::Boolean::Parse(node->Attributes["IsEnabled"]->Value);
                        xmlSettings[id] = isEnabled;
                    }
                }
            }
            catch (...) {
                System::Diagnostics::Debug::WriteLine("ArxBridge: Критическая ошибка при парсинге PropertySettings.xml");
            }
        }

        bool requiresSync = false;

        // Итерируемся по свойствам, которые принес текущий плагин
        for each (KeyValuePair<System::String^, PropertyMeta^> kvp in properties) {
            PropertyMeta^ meta = kvp.Value;
            if (meta == nullptr) continue;

            // 🚀 ЗАПОЛНЯЕМ СВОЙСТВА ДЛЯ WPF: Теперь UI сможет сгруппировать элементы по этим полям
            meta->AssemblyName = assemblyName;
            meta->ProviderClassName = className;

            // Формируем составной уникальный ID для поиска записи внутри XML файла
            System::String^ xmlLookupId = prefix + kvp.Key;

            if (xmlSettings->ContainsKey(xmlLookupId)) {
                // Если пользователь в прошлых сессиях уже менял флаг — накатываем его поверх кода плагина
                meta->IsEnabled = xmlSettings[xmlLookupId];
            }
            else {
                // Если плагин загружается впервые и свойства нет в XML, 
                // фиксируем текущее дефолтное состояние плагина, чтобы сохранить его на диск
                xmlSettings[xmlLookupId] = meta->IsEnabled;
                requiresSync = true;
            }
        }

        // 🚀 СИНХРОНИЗАЦИЯ: Если появились новые плагины/свойства, дописываем их в XML (Merge)
        if (requiresSync || !File::Exists(xmlPath)) {
            SaveAllPropertiesToXml(xmlSettings);
        }
    }

    void PropertyConfigurator::SaveAllPropertiesToXml(Dictionary<System::String^, bool>^ allSettings) {
        if (allSettings == nullptr) return;
        System::String^ xmlPath = GetXmlPath();

        try {
            XmlDocument^ doc = gcnew XmlDocument();
            XmlElement^ root = doc->CreateElement("Settings");
            doc->AppendChild(root);

            for each (KeyValuePair<System::String^, bool> kvp in allSettings) {
                XmlElement^ elem = doc->CreateElement("Property");
                elem->SetAttribute("Id", kvp.Key);
                elem->SetAttribute("IsEnabled", kvp.Value.ToString()->ToLower());
                root->AppendChild(elem);
            }

            doc->Save(xmlPath);
        }
        catch (...) {
            System::Diagnostics::Debug::WriteLine("ArxBridge: Не удалось сохранить конфигурацию свойств на диск.");
        }
    }

} // namespace ArxBridge
