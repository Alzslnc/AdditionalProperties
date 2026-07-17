#pragma once
#include "Model/ModelSupport.h"

// Подключаем базовые пространства имен .NET
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Linq;

// Подключаем пространства имен AutoCAD API (из DLL)
using namespace Autodesk::AutoCAD::DatabaseServices;

namespace ArxBridge {

    // Добавляем опережающие объявления для ОБОИХ классов:
    ref class Property;
    ref class UnitedProperty;
    ref class SelectedObjectData;
    /// <summary>
    /// Класс хранящий в себе метод получения нужных параметров и методы их изменения
    /// </summary>
    public ref class SelectedDataClass abstract
    {
            
    private:
        // Внутренние поля для свойств
        String^ _name;
        bool _changeNeed;
        int _precision;
        Dictionary<String^, PropertyMeta^>^ _allProperties;

    public:
        // Конструктор по умолчанию для инициализации полей
        SelectedDataClass()
        {
            _changeNeed = false;
            _precision = 3;
            _allProperties = gcnew Dictionary<String^, PropertyMeta^>();
        }

        /// <summary>
        /// название, используется для идентификации класса в панели настроек
        /// </summary>
        property String^ Name {
            String^ get() { return _name; }
            void set(String^ value) { _name = value; }
        }    

        /// <summary>
        /// точность округления double значений
        /// </summary>
        property int Precision {
            int get() { return _precision; }
            void set(int value) { _precision = value; }
        }

        /// <summary>
        /// Выполняет действия перед получением параметров
        /// </summary>
        virtual void PreAcadUpdateAction()
        {}

        /// <summary>
        /// Выполняет действия после получения объединения параметров
        /// </summary>
        virtual void AfterUnitedAction(UnitedProperty^ property, List<UnitedProperty^>^ properties)
        {}

        /// <summary>
       /// Поштучное получение и заполнение параметров для конкретного примитива.
       /// Вызывается C++ мостом внутри ОДНОЙ глобальной транзакции чтения.
       /// </summary>
       /// <param name="e">Живой открытый объект примитива AutoCAD (Line, Circle и др.)</param>
       /// <param name="data">Контейнер кэша, в список Properties которого наследник записывает свойства</param>
        virtual void GetProperties(Autodesk::AutoCAD::DatabaseServices::Entity^ e, SelectedObjectData^ data) abstract;

        /// <summary>
        /// Поштучное изменение параметров примитива в базе данных чертежа.
        /// Вызывается C++ мостом внутри ОДНОЙ глобальной транзакции записи (ForWrite).
        /// </summary>
        /// <param name="e">Живой открытый на запись объект примитива AutoCAD</param>
        /// <param name="properties">Список измененных свойств, которые нужно применить к объекту</param>
        virtual void SetProperties(Autodesk::AutoCAD::DatabaseServices::Entity^ e, List<Property^>^ properties) abstract;

        /// <summary>
        /// Действие, вызываемое одним из вариантов при выборе свойства из комбобокса
        /// </summary>  
        virtual bool Action(Property^ property)
        {
            return false;
        }

        /// <summary>
        /// Действие, вызываемое одним из вариантов при выборе свойства из комбобокса, возвращает какой либо объект
        /// </summary>  
        virtual Object^ ResultAction(Property^ property)
        {
            return nullptr;
        }

        /// <summary>
        /// список параметров, с флагом отображения в панели
        /// </summary>
        property Dictionary<String^, PropertyMeta^>^ AllProperties {
            Dictionary<String^, PropertyMeta^>^ get() { return _allProperties; }
            void set(Dictionary<String^, PropertyMeta^>^ value) { _allProperties = value; }
        }

        /// <summary>
        /// проверяет, содержит ли словарь хотя бы одно true значение
        /// </summary>
        bool CheckProperties()
        {
            if (_allProperties != nullptr)
            {
                for each (PropertyMeta^ meta in _allProperties->Values)
                {
                    if (meta->IsEnabled)  return true;
                }
            }               
            return false;
        }
        // 🚀 БЕЗОПАСНЫЙ ПУБЛИЧНЫЙ ДОСТУП К МЕТАДАННЫМ СВОЙСТВА ПО ИМЕНИ
        PropertyMeta^ GetPropertyMetadata(System::String^ propName) {
            if (_allProperties != nullptr && _allProperties->ContainsKey(propName)) {
                return _allProperties[propName];
            }
            return nullptr;
        }
    };
}