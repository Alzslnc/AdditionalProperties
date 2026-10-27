#pragma once
#include <vector>
#include <Unknwn.h> 
#include "Enums.h" 
#include <gcroot.h>

class AcDbObjectId;
class AcRxClass;

// 1. Опережающее объявление строго ПЕРЕД структурой
namespace ArxBridge {
    ref class PropertyMeta;
}

struct RegisteredPropertyInfo {
    gcroot<ArxBridge::PropertyMeta^> PropertyObject; // Safe CLR reference in native memory
    AcRxClass* TargetClass;
    ::IUnknown* PropertyPointer;
};

namespace ArxBridge {

  
    public enum class BridgeState
    {
        Read,
        Locked,
        Write
    };

    public ref class ArxBridgeCore sealed
    {
    internal:
        static std::vector<::IUnknown*>* m_registeredProperties;
        static std::vector<::AcDbObjectId>* m_frameSelectedIds;
        static BridgeState m_currentState;

        static std::vector<RegisteredPropertyInfo>* m_registeredProps;

        ArxBridgeCore() {}
    
    private:      
       
    public:
        static void Initialize();
        static void Terminate();
        static void SetState(BridgeState newState);
        static BridgeState GetState();

        /// <summary> Регистрация свойства для конкретных классов AutoCAD (передаем объект метаданных) </summary>
        static void RegisterDynamicPropertyForClasses(
            ArxBridge::PropertyMeta^ meta,
            System::Collections::Generic::List<System::String^>^ targetClasses
        );

        /// <summary> Регистрация общего свойства для всех AcDbEntity (передаем объект метаданных) </summary>
        static void RegisterDynamicProperty(ArxBridge::PropertyMeta^ meta);

        /// <summary> Точечное снятие регистрации конкретного свойства (сравнение по ссылке на объект) </summary>
        static void UnregisterDynamicProperty(ArxBridge::PropertyMeta^ meta);

        /// <summary> Полная очистка всех зарегистрированных мостом свойств </summary>
        static void UnregisterAllProperties();

        /// <summary> Хелпер для быстрой проверки: зарегистрирован ли этот объект прямо сейчас в AutoCAD </summary>
        static bool IsPropertyRegistered(ArxBridge::PropertyMeta^ meta);
    };
} // namespace ArxBridge
