#pragma once
#include <cmath>

inline double NativeRound(double value, int digits) {
    if (digits < 0) return value;
    double scale = std::pow(10.0, digits);
    return std::round(value * scale) / scale;
}

// 1. Создаем изолированную глобальную функцию для парсинга
inline bool SafeParseDouble(System::String^ str, double% outResult) {
    try {
        // Convert::ToDouble не содержит ломающих компилятор интерфейсов .NET 8
        outResult = System::Convert::ToDouble(str, System::Globalization::CultureInfo::InvariantCulture);
        return true;
    }
    catch (System::FormatException^) {
        outResult = 0.0;
        return false; // Строка не является числом
    }
    catch (System::OverflowException^) {
        outResult = 0.0;
        return false; // Число слишком большое/маленькое для double
    }
}