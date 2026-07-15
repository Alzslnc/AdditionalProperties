#pragma once
#include "Model/ModelSupport.h"

namespace ArxBridge {

    // 1. Предварительное объявление классов внутри их родного пространства
    ref class SelectedDataClass;
    ref class SelectedObjectData;

    // 2. Теперь BridgeManager тоже находится внутри пространства имен ArxBridge
    public ref class BridgeManager {
    private:
        // Больше не нужно писать ArxBridge:: перед типами, они находятся в той же "коробке"
        static System::Collections::Generic::List<SelectedDataClass^>^ m_providers;

    public:
        // Инициализация статических компонентов менеджера при старте
        static void Initialize();
        static void Terminate();
        // Метод регистрации C# наследников
        static void RegisterProvider(System::Object^ providerObj);
        static void RegisterProviderTest();

        // Пакетный обновитель параметров долгоживущего кэша (Void-вариант)
        static void ExecuteBatchUpdate(
            System::Collections::Generic::List<SelectedObjectData^>^ nonActualData            
        );

        static void ExecuteBatchWrite(System::Collections::Generic::List<SelectedObjectData^>^ changedData);
    };

}
