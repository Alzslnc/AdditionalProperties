#pragma once
#include <map>
#include <string>
#include "dbid.h"
#include "../Model/Enums.h" // Нужен для enum PropType

namespace ArxBridge {

    /// <summary>
    /// Сервис обслуживания стандартной панели свойств AutoCAD (OPM).
    /// Выполняет маршалинг типов данных, проверку доступности и агрегацию сумм числовых свойств.
    /// </summary>
    class OpmPropertyService
    {
    private:
        OpmPropertyService() = delete;

    public:
        // 🧮 FLYWEIGHT-СУММАТОРЫ ДЛЯ АГРЕГАЦИИ СВОЙСТВ (Перенесены сюда)
        static std::map<std::wstring, double> AggregatedDoubles;
        static std::map<std::wstring, int> AggregatedInts;

        // 📤 ПУБЛИЧНЫЕ ГЕТТЕРЫ ДАННЫХ ДЛЯ OPM ПАНЕЛИ
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue);
        static bool IsPropertyReadOnly(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outIsReadOnly);

        // 📥 ПУБЛИЧНЫЕ СЕТТЕРЫ ДАННЫХ ПРИ ВВОДЕ ПОЛЬЗОВАТЕЛЕМ ИЗ OPM
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double newValue);
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int newValue);
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, const std::wstring& newValue);

        // 🎯 ФИЛЬТРЫ И УТИЛИТЫ ИНТЕРФЕЙСА OPM
        static bool HasProperty(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName);
        static bool GetPropertyType(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, ArxBridge::PropType& outType);
    };
}
