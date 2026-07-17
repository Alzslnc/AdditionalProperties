#pragma once
#include "Model/ModelSupport.h"

namespace ArxBridge {

    // 1. Предварительное объявление классов внутри их родного пространства
    ref class SelectedDataClass;
    ref class SelectedObjectData;

    // 2. Менеджер конвейера синхронизации данных моста
    public ref class BridgeManager {
    private:
        // Хранилище зарегистрированных C# провайдеров
        static System::Collections::Generic::List<SelectedDataClass^>^ m_providers;

    public:
        // Инициализация статических компонентов менеджера при старте/выгрузке
        static void Initialize();
        static void Terminate();

        // Метод точечной регистрации C# наследников
        static void RegisterProvider(System::Object^ providerObj);

        // 🚀 ЕДИНЫЙ КОНВЕЙЕР СИНХРОНИЗАЦИИ (Запись + Чтение на месте за один проход)
        // Заменяет собой два старых метода Update и Write.
        // isWriteMode = false: Режим чистого чтения геометрии из чертежа в .NET кэш.
        // isWriteMode = true: Режим фиксации изменений из .NET кэша в чертеж с мгновенным перечитыванием.
        static void ExecuteBatchSync(
            System::Collections::Generic::List<SelectedObjectData^>^ objectsData,
            bool isWriteMode
        );  
        static PropertyMeta^ FindMetadataAcrossProviders(System::String^ propName);
       
    };

}
