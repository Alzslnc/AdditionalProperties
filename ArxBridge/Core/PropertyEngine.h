#pragma once
#include <map>
#include <vector>
#include <string>
#include "dbid.h"
#include <vcclr.h>

#include "../Model/SelectedObjectData.h"
#include "../Model/Property.h" // Подключаем, так как FindPropertyInCache возвращает тип Property^

namespace ArxBridge {

    class PropertyEngine
    {
    private:
        // 📥 БЛОК КОНТЕЙНЕРОВ КЭША ОЗУ
        static std::map<AcDbDatabase*, gcroot<System::Collections::Generic::List<SelectedObjectData^>^>> MultiDocCache;
        static gcroot<System::Collections::Generic::List<SelectedObjectData^>^> CurrentSelectionCache;

        // 🛠️ БЛОК ВСПОМОГАТЕЛЬНЫХ ЗАКРЫТЫХ МЕТОДОВ ПОИСКА
        // (Из шаблона вырезан аргумент PropType expectedType)
        template <typename T, typename ParserFunc>
        static bool TryGetCachedValue(AcDbObjectId id, const std::wstring& propName, T& outValue, ParserFunc parser);     

    public:
        // 🧮 FLYWEIGHT-СУММАТОРЫ ДЛЯ ГРУППОВОГО ВЫДЕЛЕНИЯ КМ/КЖ
        static std::map<std::wstring, double> AggregatedDoubles;
        static std::map<std::wstring, int> AggregatedInts;

    public:

        static ArxBridge::Property^ FindPropertyInCache(AcDbObjectId id, const std::wstring& propName);
        // 🔄 СИСТЕМНЫЕ МЕТОДЫ ЖИЗНЕННОГО ЦИКЛА ЯДРА
        static void Initialize();
        static void Terminate();

        // 🧹 УПРАВЛЕНИЕ ПАМЯТЬЮ И ИНВАЛИДАЦИЯ КЭША
        static void ClearCache(AcDbDatabase* pDb);
        static void InvalidateObject(AcDbDatabase* pDb, AcDbObjectId id);

        // 🚀 ГЛАВНЫЕ ДЕСАНТЫ СЧИТЫВАНИЯ И ЗАПИСИ КОНТЕКСТА
        static void PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds);
        static void PrepareInitialSelectionContext(const std::vector<AcDbObjectId>& filterTypeIds); // Наш нативный оркестратор сбора экрана через SelectImplied()
        static void CommitSelectionChanges(AcDbDatabase* pDb);

        // 📤 ПУБЛИЧНЫЕ ГЕТТЕРЫ ДАННЫХ ДЛЯ ПАЛИТРЫ (Универсальные лаконичные перегрузки)
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue);

        // 📥 ПУБЛИЧНЫЕ СЕТТЕРЫ ДАННЫХ ПРИ ВВОДЕ ПОЛЬЗОВАТЕЛЕМ
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double newValue);
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int newValue);
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, const std::wstring& newValue);

        // 🎯 ФИЛЬТРЫ И УТИЛИТЫ ИНТЕРФЕЙСА OPM
        static bool HasProperty(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName);
        static bool GetPropertyType(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, PropType& outType);
    };
}
