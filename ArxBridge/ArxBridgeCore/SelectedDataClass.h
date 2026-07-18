#pragma once
#include "PropertyMeta.h"

namespace ArxBridge {

    // Опережающие объявления для управляемых классов
    ref class Property;
    ref class SelectedObjectData;

    /// <summary>
    /// Базовый абстрактный класс моста. Служит ядром для C#-плагинов,
    /// предоставляя логику извлечения метаданных, получения и записи свойств объектов AutoCAD.
    /// </summary>
    public ref class SelectedDataClass abstract
    {
    private:
        // Используем явное указание типа System::String^ вместо короткого String^
        System::String^ _name;
        int _precision;
        System::Collections::Generic::Dictionary<System::String^, ArxBridge::PropertyMeta^>^ _allProperties;

    public:
        SelectedDataClass()
        {
            _name = System::String::Empty;
            _precision = 3;
            _allProperties = gcnew System::Collections::Generic::Dictionary<System::String^, ArxBridge::PropertyMeta^>();
        }

        property System::String^ Name {
            System::String^ get() { return _name; }
            void set(System::String^ value) { _name = value; }
        }

        property int Precision {
            int get() { return _precision; }
            void set(int value) { _precision = value; }
        }

        virtual void PreAcadUpdateAction() {}

        /// <summary>
        /// Поштучное извлечение параметров. 
        /// Обратите внимание: тип Entity указан абсолютно прозрачно и безопасно.
        /// </summary>
        virtual void GetProperties(
            Autodesk::AutoCAD::DatabaseServices::Entity^ e,
            ArxBridge::SelectedObjectData^ data) abstract;

        /// <summary>
        /// Поштучное изменение параметров.
        /// </summary>
        virtual void SetProperties(
            Autodesk::AutoCAD::DatabaseServices::Entity^ e,
            System::Collections::Generic::List<ArxBridge::Property^>^ properties) abstract;

        virtual bool Action(ArxBridge::Property^ property)
        {
            return false;
        }

        virtual Object^ ResultAction(ArxBridge::Property^ property)
        {
            return nullptr;
        }

        property System::Collections::Generic::Dictionary<System::String^, ArxBridge::PropertyMeta^>^ AllProperties {
            System::Collections::Generic::Dictionary<System::String^, ArxBridge::PropertyMeta^>^ get() { return _allProperties; }
            void set(System::Collections::Generic::Dictionary<System::String^, ArxBridge::PropertyMeta^>^ value) { _allProperties = value; }
        }    

        ArxBridge::PropertyMeta^ GetPropertyMetadata(System::String^ propName) {
            if (_allProperties != nullptr && _allProperties->ContainsKey(propName)) {
                return _allProperties[propName];
            }
            return nullptr;
        }
    };
}
