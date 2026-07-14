#pragma once
#include <map>
#include <vector>
#include <string>
#include "dbid.h"
#include <vcclr.h>

// Подключаем модель строго по иерархии проекта
#include "../Model/SelectedObjectData.h"

namespace ArxBridge {

    class PropertyEngine
    {
    private:
        // 1. Долгоживущий многодокументный кэш: База_Данных -> Список управляемых DTO-объектов
        // Убрали лишние префиксы ArxBridge:: перед SelectedObjectData
        static std::map<AcDbDatabase*, gcroot<System::Collections::Generic::List<SelectedObjectData^>^>> MultiDocCache;

        // 2. Короткоживущий список объектов ТЕКУЩЕГО выбора пользователя (для геттеров палитры)
        static gcroot<System::Collections::Generic::List<SelectedObjectData^>^> CurrentSelectionCache;

        template <typename T, typename ParserFunc>
        static bool TryGetCachedValue(AcDbObjectId id, const std::wstring& propName, PropType expectedType, T& outValue, ParserFunc parser);

    public:
        // 3. Короткоживущие Flyweight-агрегаторы числовых данных КМ/КЖ (существуют в рамках одного выбора)
        static std::map<std::wstring, double> AggregatedDoubles;
        static std::map<std::wstring, int> AggregatedInts;

    public:
        static void Initialize();
        static void Terminate();

        // Управление кэшем на основе событий из PropertyReactor
        static void ClearCache(AcDbDatabase* pDb);
        static void InvalidateObject(AcDbDatabase* pDb, AcDbObjectId id);

        // ГЛАВНЫЙ СИНХРОННЫЙ ПАКЕТНЫЙ КОНВЕЙЕР: Вызывается при обновлении/выборе элементов
        static void PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds);

        // Динамические геттеры ленивого чтения данных для ArxBridgeCore.cpp (COM-моста палитры)
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue);
    };
}
