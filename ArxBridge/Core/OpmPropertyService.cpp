#include "OpmPropertyService.h"
#include "DocumentCacheManager.h"
#include "../Model/SelectedObjectData.h"
#include "../Model/Property.h"
#include "../Model/SelectedDataClass.h"
#include <sstream>
#include <iomanip>

using namespace System;
using namespace System::Collections::Generic;

namespace {
    // Внутренняя изолированная лямбда-функция для красивого форматирования double без дублирования кода (DRY)
    std::wstring FormatDoubleWithPrecision(double value, int precision) {
        std::wstringstream formatStream;
        formatStream.imbue(std::locale::classic()); // Защита разделителя (точки)
        formatStream << std::fixed << std::setprecision(precision) << value;
        return formatStream.str();
    }
}

namespace ArxBridge {

    // Выделение памяти под статические сумматоры группового выбора КМ/КЖ
    std::map<std::wstring, double> OpmPropertyService::AggregatedDoubles;
    std::map<std::wstring, int> OpmPropertyService::AggregatedInts;

    // --- 1. ПУБЛИЧНЫЕ ГЕТТЕРЫ ДАННЫХ ДЛЯ OPM ПАНЕЛИ ---

    bool OpmPropertyService::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue) {
        // Проверяем агрегированную сумму группового выбора
        auto itSum = AggregatedDoubles.find(propName);
        if (itSum != AggregatedDoubles.end()) {
            outValue = itSum->second;
            return true;
        }

        Property^ propInCache = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (propInCache == nullptr || propInCache->Value == nullptr) return false;

        try {
            // Вычитываем и парсим строку из ультра-легковесного Property
            pin_ptr<const wchar_t> wVal = PtrToStringChars(propInCache->Value);
            std::wstringstream parseStream(wVal);
            parseStream.imbue(std::locale::classic());

            double rawDoubleValue = 0.0;
            if (parseStream >> rawDoubleValue) {
                int displayPrecision = (propInCache->Parent != nullptr) ? propInCache->Parent->Precision : 6;
                double factor = std::pow(10.0, displayPrecision);
                outValue = std::round(rawDoubleValue * factor) / factor;
                return true;
            }
        }
        catch (...) {}
        return false;
    }

    bool OpmPropertyService::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue) {
        auto itSum = AggregatedInts.find(propName);
        if (itSum != AggregatedInts.end()) {
            outValue = itSum->second;
            return true;
        }

        Property^ propInCache = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (propInCache == nullptr || propInCache->Value == nullptr) return false;

        try {
            pin_ptr<const wchar_t> wVal = PtrToStringChars(propInCache->Value);
            std::wstringstream parseStream(wVal);
            parseStream.imbue(std::locale::classic());
            if (parseStream >> outValue) return true;
        }
        catch (...) {}
        return false;
    }

    bool OpmPropertyService::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outValue) {
        Property^ prop = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (prop == nullptr || prop->Value == nullptr) return false;

        // ИСПРАВЛЕН КРИТИЧЕСКИЙ БАГ: Теперь метод возвращает само логическое значение, а не IsReadOnly!
        pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
        std::wstring valStr(wVal);
        outValue = (valStr == L"True" || valStr == L"true" || valStr == L"1");
        return true;
    }

    bool OpmPropertyService::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue) {
        int displayPrecision = 6;
        bool isDoubleType = false;

        Property^ propInCache = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (propInCache != nullptr) {
            isDoubleType = (propInCache->PropType == PropType::IsDouble);
            if (propInCache->Parent != nullptr) {
                displayPrecision = propInCache->Parent->Precision;
            }
        }

        // 1. Ищем в double-сумматорах
        auto itDouble = AggregatedDoubles.find(propName);
        if (itDouble != AggregatedDoubles.end()) {
            outValue = FormatDoubleWithPrecision(itDouble->second, displayPrecision);
            return true;
        }

        // 2. Ищем in int-сумматорах
        auto itInt = AggregatedInts.find(propName);
        if (itInt != AggregatedInts.end()) {
            outValue = std::to_wstring(itInt->second);
            return true;
        }

        // 3. Одиночный выбор или обычная строка
        if (propInCache != nullptr && propInCache->Value != nullptr) {
            pin_ptr<const wchar_t> wVal = PtrToStringChars(propInCache->Value);
            outValue = std::wstring(wVal);

            if (isDoubleType && !outValue.empty()) {
                try {
                    std::wstringstream parseStream(outValue);
                    parseStream.imbue(std::locale::classic());
                    double parsedDouble = 0.0;
                    if (parseStream >> parsedDouble) {
                        outValue = FormatDoubleWithPrecision(parsedDouble, displayPrecision);
                    }
                }
                catch (...) {}
            }
            return true;
        }
        return false;
    }
    bool OpmPropertyService::IsPropertyReadOnly(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outIsReadOnly) {
        Property^ prop = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (prop == nullptr) return false;

        // Забираем честный флаг защиты, вычисленный C# плагином
        outIsReadOnly = prop->IsReadOnly;
        return true;
    }

    // --- 2. ПУБЛИЧНЫЕ СЕТТЕРЫ ДАННЫХ ПРИ ВВОДЕ ПОЛЬЗОВАТЕЛЕМ ИЗ OPM ---

    bool OpmPropertyService::SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double newValue) {
        Property^ prop = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (prop == nullptr || prop->IsReadOnly || prop->PropType != PropType::IsDouble) return false;

        // БЕЗОПАСНЫЙ МАРШАЛИНГ: Переводим нативное число в .NET строку инвариантно
        prop->NewValue = System::Convert::ToString(newValue, System::Globalization::CultureInfo::InvariantCulture);
        return true;
    }

    bool OpmPropertyService::SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int newValue) {
        Property^ prop = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (prop == nullptr || prop->IsReadOnly || prop->PropType != PropType::IsInteger) return false;

        prop->NewValue = System::Convert::ToString(newValue);
        return true;
    }

    bool OpmPropertyService::SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, const std::wstring& newValueRaw) {
        Property^ prop = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (prop == nullptr || prop->IsReadOnly) return false;

        std::wstring normalizedStr = newValueRaw;
        size_t commaPos = normalizedStr.find(L',');
        if (commaPos != std::wstring::npos) {
            normalizedStr[commaPos] = L'.'; // Нормализуем запятые в точки для безопасности
        }

        try {
            std::wstringstream checkStream(normalizedStr);
            checkStream.imbue(std::locale::classic());
            if (prop->PropType == PropType::IsDouble) {
                double dummy;
                if (!(checkStream >> dummy) || !checkStream.eof()) return false;
            }
            else if (prop->PropType == PropType::IsInteger) {
                int dummy;
                if (!(checkStream >> dummy) || !checkStream.eof()) return false;
            }
        }
        catch (...) { return false; }

        prop->NewValue = gcnew System::String(newValueRaw.c_str());
        return true;
    }

    // --- 3. ФИЛЬТРЫ И УТИЛИТЫ ИНТЕРФЕЙСА OPM ---

    bool OpmPropertyService::HasProperty(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName) {
        return (DocumentCacheManager::FindPropertyInCache(pDb, id, propName) != nullptr);
    }

    bool OpmPropertyService::GetPropertyType(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, PropType& outType) {
        Property^ prop = DocumentCacheManager::FindPropertyInCache(pDb, id, propName);
        if (prop != nullptr) {
            outType = prop->PropType;
            return true;
        }
        return false;
    }
}
