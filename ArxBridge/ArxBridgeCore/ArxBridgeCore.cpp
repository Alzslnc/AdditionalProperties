#include <windows.h>
#include "ArxBridgeCore.h"
#include "arxHeaders.h" 
#include "PropertyMeta.h" 
#include "dbapserv.h"                  
#include "dbmain.h"                    
#include <dynprops.h> 

namespace ArxBridge {
   
    // Опережающее объявление нашей нативной фабрики из соседнего файла реализации
    ::IUnknown* CreateDynamicProperty(System::String^ name, System::String^ description, ArxBridge::PropType type);

    void ArxBridgeCore::Initialize() {
        // 2. Выделяем память под вектор в нативной куче
        if (m_registeredProps == nullptr) {
            m_registeredProps = new std::vector<RegisteredPropertyInfo>();
        }
        else {
            m_registeredProps->clear();
        }


        if (m_frameSelectedIds == nullptr) {
            m_frameSelectedIds = new std::vector<AcDbObjectId>();
        }
        m_currentState = BridgeState::Read;
    }

    void ArxBridgeCore::Terminate() {
        UnregisterAllProperties();
        if (m_frameSelectedIds != nullptr) {
            delete m_frameSelectedIds;
            m_frameSelectedIds = nullptr;
        }
    }

    void ArxBridgeCore::SetState(BridgeState newState) { m_currentState = newState; }
    BridgeState ArxBridgeCore::GetState() { return m_currentState; }

    // 1. Регистрация для точечных классов объектов
    void ArxBridgeCore::RegisterDynamicPropertyForClasses(
        ArxBridge::PropertyMeta^ meta,
        System::Collections::Generic::List<System::String^>^ targetClasses)
    {
        if (meta == nullptr || targetClasses == nullptr || targetClasses->Count == 0 || m_registeredProps == nullptr) return;

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
                // 🛡️ ПРЕДОХРАНИТЕЛЬ: Сверяем ссылку на объект в куче .NET для конкретного класса
                bool alreadyExists = false;
                for (size_t i = 0; i < m_registeredProps->size(); ++i) {
                    const RegisteredPropertyInfo& existingInfo = (*m_registeredProps)[i];
                    // gcroot автоматически приводится к управляемому типу при сравнении
                    if (static_cast<ArxBridge::PropertyMeta^>(existingInfo.PropertyObject) == meta && existingInfo.TargetClass == pTargetClass) {
                        alreadyExists = true;
                        break;
                    }
                }

                if (alreadyExists) continue; // Данный объект для данного класса уже в AutoCAD, пропускаем

                // Передаем параметры строки из объекта 'meta' в нативную фабрику
                ::IUnknown* pUnkProp = CreateDynamicProperty(meta->Name, meta->Description, meta->PropType);
                if (pUnkProp == nullptr) continue;

                pUnkProp->AddRef();

                IPropertyManager2* pTargetPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(pTargetClass);
                if (pTargetPropMan != nullptr) {
                    pTargetPropMan->AddProperty(pUnkProp);

                    RegisteredPropertyInfo info;
                    info.PropertyObject = meta; // Фиксируем саму ссылку на объект через gcroot
                    info.TargetClass = pTargetClass;
                    info.PropertyPointer = pUnkProp;
                    m_registeredProps->push_back(info);
                }
                else {
                    pUnkProp->Release();
                }
            }
        }
    }

    // 2. Регистрация общего свойства (для всех AcDbEntity)
    void ArxBridgeCore::RegisterDynamicProperty(ArxBridge::PropertyMeta^ meta)
    {
        if (meta == nullptr || m_registeredProps == nullptr) return;

        AcRxClass* pDefaultClass = AcDbEntity::desc();
        IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(pDefaultClass);
        if (pPropMan == nullptr) return;

        // 🛡️ ПРЕДОХРАНИТЕЛЬ: Сверяем ссылку на базовом классе сущностей
        for (size_t i = 0; i < m_registeredProps->size(); ++i) {
            const RegisteredPropertyInfo& existingInfo = (*m_registeredProps)[i];
            if (static_cast<ArxBridge::PropertyMeta^>(existingInfo.PropertyObject) == meta && existingInfo.TargetClass == pDefaultClass) {
                return; // Объект свойства уже добавлен, выходим
            }
        }

        ::IUnknown* pUnkProp = CreateDynamicProperty(meta->Name, meta->Description, meta->PropType);
        if (pUnkProp == nullptr) return;

        pUnkProp->AddRef();
        pPropMan->AddProperty(pUnkProp);

        RegisteredPropertyInfo info;
        info.PropertyObject = meta;
        info.TargetClass = pDefaultClass;
        info.PropertyPointer = pUnkProp;
        m_registeredProps->push_back(info);
    }

    // 3. Снятие регистрации с конкретного свойства (По ссылке)
    void ArxBridgeCore::UnregisterDynamicProperty(ArxBridge::PropertyMeta^ meta)
    {
        if (meta == nullptr || m_registeredProps == nullptr) return;

        // Идем с конца вектора (используя знаковые индексы int), чтобы безопасно удалять в процессе
        for (int i = static_cast<int>(m_registeredProps->size()) - 1; i >= 0; --i)
        {
            const RegisteredPropertyInfo& info = (*m_registeredProps)[i];

            // 🚀 СВЕРКА ПО РЕФЕРЕНСУ: gcroot неявно преобразуется к PropertyMeta^
            if (static_cast<ArxBridge::PropertyMeta^>(info.PropertyObject) == meta) 
            {
                IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(info.TargetClass);
                if (pPropMan != nullptr)
                {
                    pPropMan->RemoveProperty(info.PropertyPointer);
                }

                if (info.PropertyPointer != nullptr) {
                    info.PropertyPointer->Release();
                }

                // Удаляем запись из вектора по итератору
                m_registeredProps->erase(m_registeredProps->begin() + i);
            }
        }
    }

    // 4. Полное снятие всех свойств при Terminate
    void ArxBridgeCore::UnregisterAllProperties()
    {
        if (m_registeredProps == nullptr) return;

        for (size_t i = 0; i < m_registeredProps->size(); ++i)
        {
            const RegisteredPropertyInfo& info = (*m_registeredProps)[i];
            IPropertyManager2* pPropMan = (IPropertyManager2*)GET_OPMPROPERTY_MANAGER(info.TargetClass);
            if (pPropMan != nullptr)
            {
                pPropMan->RemoveProperty(info.PropertyPointer);
            }

            if (info.PropertyPointer != nullptr) {
                info.PropertyPointer->Release();
            }
        }
        m_registeredProps->clear();
    }

    // 5. Реализация быстрого хелпера для BridgeManager
    bool ArxBridgeCore::IsPropertyRegistered(ArxBridge::PropertyMeta^ meta)
    {
        if (meta == nullptr || m_registeredProps == nullptr) return false;

        for (size_t i = 0; i < m_registeredProps->size(); ++i) {
            if (static_cast<ArxBridge::PropertyMeta^>((*m_registeredProps)[i].PropertyObject) == meta) {
                return true; // Свойство найдено в реестре AutoCAD, значит оно активно
            }
        }
        return false;
    }

} // namespace ArxBridge
