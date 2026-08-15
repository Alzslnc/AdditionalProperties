#pragma once
#include <vector>
#include <Unknwn.h> 
#include "Enums.h" 

class AcDbObjectId;

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
        ArxBridgeCore() {}

    public:
        static void Initialize();
        static void Terminate();
        static void SetState(BridgeState newState);
        static BridgeState GetState();

        static void RegisterDynamicPropertyForClasses(
            System::String^ propertyName,
            System::String^ description,
            ArxBridge::PropType type,
            System::Collections::Generic::List<System::String^>^ targetClasses
        );

        static void RegisterDynamicProperty(System::String^ propertyName, System::String^ propertyDescription, ArxBridge::PropType propType);
        static void UnregisterAllProperties();
    };
} // namespace ArxBridge
