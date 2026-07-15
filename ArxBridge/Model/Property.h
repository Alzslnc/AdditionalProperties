#pragma once
#include "Model/ModelSupport.h"

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    ref class SelectedDataClass;

    /// <summary>
    /// Компонент сложного (составного) значения параметра.
    /// </summary>
    public ref class ValueComponent
    {
    public:
        property String^ Value;
    };

    /// <summary>
    /// Класс индивидуального и совмещенного свойства объекта AutoCAD.
    /// Полностью очищен от Object^ в полях значений для защиты компилятора в gcroot.h.
    /// </summary>
    public ref class Property
    {
    private:
        bool _isReadOnly;
        String^ _value;
        String^ _newValue;
        bool _fullUpdateNeed;
        bool _isSum;
        String^ _name;
        String^ _description;
        String^ _objectName;
        String^ _group;
        SelectedDataClass^ _parent;

        // Поля флагов графического интерфейса
        bool _showVarious;
        bool _error;
        bool _isAction;
        bool _isSelected;
        bool _isVarious;
        bool _objectTypeEqual;
        bool _needRegen;
        List<ValueComponent^>^ _components;

    public:
        // --- Конструкторы класса (Только объявления) ---
        Property();
        Property(String^ name, String^ description, String^ value, bool isReadOnly, ArxBridge::PropType propType, SelectedDataClass^ parent);

        // --- Методы жизненного цикла и сравнения ---
        bool IsEqual(Property^ property);
        bool IsEqualValue(String^ value);
        bool IsEqualValue(Property^ property);
        Property^ Clone();

        // --- Управляемые свойства (Доступ к инкапсулированным полям) ---
        property String^ Name {
            String^ get() { return _name; }
            void set(String^ value) { _name = value; }
        }

        property String^ Description{
                      String ^ get() { return _description; }
                      void set(String ^ value) { _description = value; }
        }

        property String^ ObjectName {
            String^ get() { return _objectName; }
            void set(String^ value) { _objectName = value; }
        }

        property String^ Group {
            String^ get() { return _group; }
            void set(String^ value) { _group = value; }
        }

        property SelectedDataClass^ Parent {
            SelectedDataClass^ get() { return _parent; }
            void set(SelectedDataClass^ value) { _parent = value; }
        }

        property PropType PropType;

        property bool IsSum {
            bool get() { return _isSum; }
            void set(bool value) { _isSum = value; }
        }

        property bool IsReadOnly {
            bool get() { return _isSum ? true : _isReadOnly; }
            void set(bool value) { _isReadOnly = value; }
        }

        property String^ Value {
            String^ get() { return _value; }
            void set(String^ value) { _value = value; }
        }

        property String^ NewValue {
            String^ get() {
                // Возвращает ровно то, что сохранено (nullptr или строку)
                return _newValue;
            }
            void set(String^ value) {
                // Сохраняет ровно то, что пришло (даже если пришел nullptr)
                _newValue = value;
            }
        }

        property bool IsChanged {
            bool get() 
            { 
                if (NewValue == nullptr || Value == nullptr) return false;
                return !IsEqualValue(NewValue);
            }         
        }

        property bool FullUpdateNeed {
            bool get() { return _fullUpdateNeed; }
            void set(bool value) { _fullUpdateNeed = value; }
        }

        // Свойства расширенных флагов интерфейса палитры
        property bool ShowVarious {
            bool get() { return _showVarious; }
            void set(bool value) { _showVarious = value; }
        }

        property bool Error {
            bool get() { return _error; }
            void set(bool value) { _error = value; }
        }

        property bool IsAction {
            bool get() { return _isAction; }
            void set(bool value) { _isAction = value; }
        }

        property bool IsSelected {
            bool get() { return _isSelected; }
            void set(bool value) { _isSelected = value; }
        }

        property bool IsVarious {
            bool get() { return _isVarious; }
            void set(bool value) { _isVarious = value; }
        }

        property bool ObjectTypeEqual {
            bool get() { return _objectTypeEqual; }
            void set(bool value) { _objectTypeEqual = value; }
        }

        property bool NeedRegen {
            bool get() { return _needRegen; }
            void set(bool value) { _needRegen = value; }
        }

        property List<ValueComponent^>^ Components {
            List<ValueComponent^>^ get() { return _components; }
            void set(List<ValueComponent^>^ value) { _components = value; }
        }
    };
}
