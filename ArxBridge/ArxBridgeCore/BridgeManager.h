#pragma once
#include "PropertyMeta.h" // 👈 ИСПРАВЛЕНО: Подключаем вместо удаленного ModelSupport.h

namespace ArxBridge {

    // Предварительное объявление управляемых классов для изоляции заголовка
    ref class SelectedDataClass;
    ref class SelectedObjectData;

    /// <summary>
    /// Главный менеджер конвейера синхронизации данных моста.
    /// Управляет реестром C#-провайдеров (плагинов) и координирует пакетный маршалинг данных 
    /// между нативными объектами AutoCAD и управляемым .NET кэшем свойств.
    /// </summary>
    public ref class BridgeManager {
    private:
        // Хранилище зарегистрированных C# провайдеров
        static System::Collections::Generic::List<SelectedDataClass^>^ m_providers;

    public:
        /// <summary> Инициализация статических компонентов менеджера при старте плагина. </summary>
        static void Initialize();

        /// <summary> Очистка памяти и освобождение ресурсов при выгрузке плагина. </summary>
        static void Terminate();

        /// <summary> Метод динамической регистрации C#-наследников (плагинов) в нативном слое моста. </summary>
        static void RegisterProvider(System::Object^ providerObj);

        /// <summary>
        /// ЕДИНЫЙ ПАКЕТНЫЙ КОНВЕЙЕР СИНХРОНИЗАЦИИ (Запись + Чтение на месте за один проход).
        /// Открывает транзакцию AutoCAD и веерно передает объекты в C#-плагины.
        /// </summary>
        /// <param name="objectsData">Пакет объектов из кэша, требующих актуализации или фиксации.</param>
        /// <param name="isWriteMode">
        /// false: Режим чистого пакетного чтения геометрии из чертежа в .NET кэш.
        /// true: Режим фиксации измененных в OPM свойств в чертеж (ForWrite) с мгновенным обновлением кэша.
        /// </param>
        static void ExecuteBatchSync(
            System::Collections::Generic::List<ArxBridge::SelectedObjectData^>^ objectsData,
            bool isWriteMode
        );

        /// <summary> Сквозной поиск паспорта метаданных свойства по всем зарегистрированным C#-плагинам. </summary>
        static ArxBridge::PropertyMeta^ FindMetadataAcrossProviders(System::String^ propName);
    };

} // namespace ArxBridge
