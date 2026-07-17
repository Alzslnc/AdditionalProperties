#include <cmath> 
#include "Property.h"            
#include "SelectedDataClass.h"   
#include "Support.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace Autodesk::AutoCAD::DatabaseServices;
using namespace System::Globalization;

namespace ArxBridge {

    // --- 1. Конструктор по умолчанию (ОДИН экземпляр) ---
    Property::Property()
    {
        _isReadOnly = false;
        _value = String::Empty;
        _newValue = String::Empty;        
        _fullUpdateNeed = false;
        _isSum = false;
        _name = String::Empty;
        _objectName = String::Empty;     
        _parent = nullptr;
        PropType = ArxBridge::PropType::IsString;

        _showVarious = true;
        _error = false;
        _isAction = false;
        _isSelected = false;
        _isVarious = false;
        _objectTypeEqual = false;
        _needRegen = false;
        _components = gcnew List<ValueComponent^>();
    }

    // --- 2. Параметризованный конструктор модулей КМ/КЖ ---
    Property::Property(PropertyMeta^ meta, String^ value, bool isReadOnly, SelectedDataClass^ parent)
    {
        _name = meta->Name;
        _description = meta->Description;
        _value = value;
        _isReadOnly = isReadOnly;
        _parent = parent;
        _newValue = nullptr;      
        _fullUpdateNeed = false;
        _isSum = false;
        _objectName = String::Empty;      
        PropType = meta->PropType;

        _showVarious = true;
        _error = false;
        _isAction = false;
        _isSelected = false;
        _isVarious = false;
        _objectTypeEqual = false;
        _needRegen = false;
        _components = gcnew List<ValueComponent^>();
    }


    // --- 4. Реализация бизнес-методов сравнения ---
    bool Property::IsEqual(Property^ property)
    {
        if (property == nullptr) return false;
        if (Name != property->Name) return false;
        if (PropType != property->PropType) return false;       
        if (IsSum != property->IsSum) return false;
        if (Parent == nullptr || property->Parent == nullptr) return false;
        if (Parent != property->Parent) return false;

        // Сравниваем строковые значения напрямую
        if (Value != property->Value) return false;

        return true;
    }

    bool Property::IsEqualValue(String^ value) {
        if (_isVarious) return false;
        if (value == nullptr || Value == nullptr) return false;
        if (_parent == nullptr) return false;

        int precision = _parent->Precision;

        String^ s1 = Value->ToString()->Replace(",", ".");
        String^ s2 = value->ToString()->Replace(",", ".");

        double result1;
        // ИСПРАВЛЕНО: Убрано дублирование "double double result2"
        double result2;

        if (SafeParseDouble(s1, result1) && SafeParseDouble(s2, result2))
        {
            double factor = std::pow(10.0, precision);
            double rounded1 = std::round(result1 * factor) / factor;
            double rounded2 = std::round(result2 * factor) / factor;

            return rounded1 == rounded2;
        }

        return s1 == s2;
    }

    bool Property::IsEqualValue(Property^ property) {
        if (property == nullptr) return false;
        return IsEqualValue(property->Value);
    }

    // --- 5. Глубокое и безопасное клонирование объектов ---
    Property^ Property::Clone()
    {
        Property^ copy = gcnew Property();
        copy->Name = this->Name;
        copy->ObjectName = this->ObjectName;      
        copy->IsSum = this->IsSum;
        copy->IsReadOnly = this->_isReadOnly;
        copy->Parent = this->Parent;
        copy->Value = this->Value;
        copy->NewValue = this->NewValue;
        copy->FullUpdateNeed = this->FullUpdateNeed;    
        copy->PropType = this->PropType;

        copy->ShowVarious = this->ShowVarious;
        copy->Error = this->Error;
        copy->IsAction = this->IsAction;
        copy->IsSelected = this->IsSelected;
        copy->IsVarious = this->IsVarious;
        copy->ObjectTypeEqual = this->ObjectTypeEqual;
        copy->NeedRegen = this->NeedRegen;
        return copy;
    }
}
