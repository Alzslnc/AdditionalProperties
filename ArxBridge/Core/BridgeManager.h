#pragma once
#include <vcclr.h>

namespace ArxBridge {
    ref class SelectedDataClass;
    ref class SelectedObjectData;
}

class BridgeManager {
private:
    // Управляемый реестр зарегистрированных C# плагинов КМ/КЖ
    static gcroot<System::Collections::Generic::List<ArxBridge::SelectedDataClass^>^> m_providers;

public:
    // Инициализация статических компонентов менеджера при старте
    static void Initialize();

    // Метод регистрации C# наследников
    static void RegisterProvider(ArxBridge::SelectedDataClass^ provider);

    // Пакетный обновитель параметров долгоживущего кэша (Void-вариант)
    static void ExecuteBatchUpdate(
        System::Collections::Generic::List<ArxBridge::SelectedObjectData^>^ nonActualData
    );
};
