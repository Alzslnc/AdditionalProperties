#pragma once

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {
    // Опережающее объявление управляемого класса метаданных динамического свойства
    ref class PropertyMeta;

    /// <summary>
    /// Внутреннее ядро моста для актуализации и сохранения пользовательских настроек видимости свойств.
    /// Автоматически разделяет свойства по сборкам (DLL) во избежание коллизий имен.
    /// </summary>
    public ref class PropertyConfigurator {
    public:
        /// <summary>
        /// Считывает XML-конфигурацию с диска и актуализирует флаги IsEnabled внутри метаданных провайдера.
        /// Также заполняет свойства AssemblyName и ProviderClassName для последующей группировки в WPF UI.
        /// </summary>
        /// <param name="providerObj">Управляемый инстанс класса-провайдера (SelectedDataClass^).</param>
        /// <param name="properties">Словарь метаданных свойств этого провайдера.</param>
        static void UpdatePropertiesFromXml(System::Object^ providerObj, Dictionary<System::String^, PropertyMeta^>^ properties);

        /// <summary>
        /// Возвращает абсолютный путь к файлу конфигурации PropertySettings.xml в папке AppData пользователя.
        /// </summary>
        static System::String^ GetXmlPath();
        // Внутренний метод для перезаписи XML на основе объединенной карты настроек со всех плагинов
        static void SaveAllPropertiesToXml(Dictionary<System::String^, bool>^ allSettings);
    private:
        
    };
}
