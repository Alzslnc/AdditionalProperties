#include <vector>

namespace ArxBridge {
       
    // Обязательно public ref class, чтобы его увидели C# модули и BridgeManager!
    public ref class ArxBridgeCore sealed
    {
    internal:
        // Нативный контейнер (std::vector) храним под флагом internal. 
        // C# его не увидит, но внутри этого же C++/CLI проекта (в BridgeManager) им можно пользоваться.
        static std::vector<IUnknown*>* m_registeredProperties = new std::vector<IUnknown*>();

    public:
        // Метод регистрации, который теперь принимает управляемую строку
        static void RegisterDynamicProperty(System::String^ propertyName);

        // Массовая выгрузка
        static void UnregisterAllProperties();
    };
}
