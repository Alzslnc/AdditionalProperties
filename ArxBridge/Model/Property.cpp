#include "Property.h"
#include "PropertyMeta.h"
#include "SelectedDataClass.h"
#include <cmath>

using namespace System;

namespace {
    bool SafeParseDouble(System::String^ str, double% outResult) {
        try {
            outResult = System::Convert::ToDouble(str, System::Globalization::CultureInfo::InvariantCulture);
            return true;
        }
        catch (System::FormatException^) { outResult = 0.0; return false; }
        catch (System::OverflowException^) { outResult = 0.0; return false; }
    }
}

namespace ArxBridge {

    // --- 1. Конструктор по умолчанию ---
    Property::Property()
    {
        _name = String::Empty;
        _value = String::Empty;
        _newValue = nullptr;
        _propType = ArxBridge::PropType::IsString; // Задаем базовый тип по умолчанию
        _isReadOnly = false;
        _isSum = false;
        _error = false;
        _parent = nullptr;
    }

    // --- 2. Параметризованный конструктор ---
    Property::Property(PropertyMeta^ meta, String^ value, bool isReadOnly, SelectedDataClass^ parent)
    {
        _name = (meta != nullptr) ? meta->Name : String::Empty;
        _propType = (meta != nullptr) ? meta->PropType : ArxBridge::PropType::IsString; // Кэшируем тип данных из меты
        _value = value;
        _newValue = nullptr;
        _isReadOnly = isReadOnly;
        _isSum = false;
        _parent = parent;
    }

    // --- 3. Внутреннее сравнение значений с округлением по Precision родителя ---
    bool Property::IsEqualValue(String^ value)
    {
        if (value == nullptr || _value == nullptr) return false;
        if (_parent == nullptr) return false;

        String^ s1 = _value->Replace(",", ".");
        String^ s2 = value->Replace(",", ".");

        double result1;
        double result2;

        if (SafeParseDouble(s1, result1) && SafeParseDouble(s2, result2))
        {
            int digits = _parent->Precision;

            if (digits >= 0) {
                double scale = std::pow(10.0, digits);
                double rounded1 = std::round(result1 * scale) / scale;
                double rounded2 = std::round(result2 * scale) / scale;
                return rounded1 == rounded2;
            }
            return result1 == result2;
        }

        return String::Equals(s1, s2, StringComparison::OrdinalIgnoreCase);
    }
}
