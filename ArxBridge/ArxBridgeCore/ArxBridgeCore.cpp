#include <windows.h>
#include "ArxBridgeCore.h"
#include "arxHeaders.h" 
#include "dbapserv.h"                  
#include "dbmain.h"                    
#include <dynprops.h> 

namespace ArxBridge {

    // Опережающее объявление нашей нативной фабрики из соседнего файла реализации
    ::IUnknown* CreateDynamicProperty(System::String^ name, System::String^ description, ArxBridge::PropType type);

    void ArxBridgeCore::Initialize() {
        if (m_registeredProperties == nullptr) {
            m_registeredProperties = new std::vector<IUnknown*>();
        }
        if (m_frameSelectedIds == nullptr) {
            m_frameSelectedIds = new std::vector<AcDbObjectId>();
        }
        m_currentState = BridgeState::Read;
    }

    void ArxBridgeCore::Terminate() {
        UnregisterAllProperties();
        if (m_registeredProperties != nullptr) { delete m_registeredProperties; m_registeredProperties = nullptr; }
        if (m_frameSelectedIds != nullptr) { delete m_frameSelectedIds; m_frameSelectedIds = nullptr; }
    }

    void ArxBridgeCore::SetState(BridgeState newState) { m_currentState = newState; }
    BridgeState ArxBridgeCore::GetState() { return m_currentState; }

    void ArxBridgeCore::RegisterDynamicPropertyForClasses(
        System::String^ propertyName,
        System::String^ description,
        PropType type,
        System::Collections::Generic::List<System::String^>^ targetClasses)
    {
        if (System::String::IsNullOrEmpty(propertyName) || targetClasses == nullptr || targetClasses->Count == 0) return;

        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());
        if (pPropMan == nullptr) return;

        for each (System::String ^ managedClassName in targetClasses)
        {
            if (System::String::IsNullOrEmpty(managedClassName)) continue;

            System::IntPtr ipClass = System::Runtime::InteropServices::Marshal::StringToHGlobalUni(managedClassName);
            const wchar_t* nativeClassName = static_cast<const wchar_t*>(ipClass.ToPointer());

            AcRxClass* pTargetClass = nullptr;
            if (acrxClassDictionary != nullptr) {
                pTargetClass = AcRxClass::cast(acrxClassDictionary->at(nativeClassName));
            }

            System::Runtime::InteropServices::Marshal::FreeHGlobal(ipClass);

            if (pTargetClass != nullptr)
            {
                // 🚀 ИСПРАВЛЕНО: Вызываем фабрику вместо прямого оператора new
                ::IUnknown* pUnkProp = CreateDynamicProperty(propertyName, description, type);
                if (pUnkProp == nullptr) continue;

                pUnkProp->AddRef();

                IPropertyManager2* pTargetPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(pTargetClass);
                if (pTargetPropMan != nullptr) {
                    pTargetPropMan->AddProperty(pUnkProp);
                }
                m_registeredProperties->push_back(pUnkProp);
            }
        }
    }

    void ArxBridgeCore::RegisterDynamicProperty(System::String^ propertyName, System::String^ propertyDescription, ArxBridge::PropType propType)
    {
        if (System::String::IsNullOrEmpty(propertyName)) return;

        // 🚀 ИСПРАВЛЕНО: Вызываем фабрику вместо прямого оператора new
        ::IUnknown* pUnkProp = CreateDynamicProperty(propertyName, propertyDescription, propType);
        if (pUnkProp == nullptr) return;

        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());

        if (pPropMan != nullptr)
        {
            pUnkProp->AddRef();
            pPropMan->AddProperty(pUnkProp);
            m_registeredProperties->push_back(pUnkProp);
        }
        else
        {
            pUnkProp->Release();
        }
    }

    void ArxBridgeCore::UnregisterAllProperties()
    {
        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(AcDbEntity::desc());
        if (pPropMan != nullptr && m_registeredProperties != nullptr)
        {
            for (IUnknown* pProp : *m_registeredProperties)
            {
                pPropMan->RemoveProperty(pProp);
                pProp->Release();
            }
        }
        if (m_registeredProperties != nullptr) m_registeredProperties->clear();
    }
} // namespace ArxBridge
