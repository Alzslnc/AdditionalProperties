#pragma once
#include <map>
#include <vector>
#include <string>
#include "dbid.h"
#include <vcclr.h>

#include "../Model/SelectedObjectData.h"

namespace ArxBridge {

    class PropertyEngine
    {
    private:
        static std::map<AcDbDatabase*, gcroot<System::Collections::Generic::List<SelectedObjectData^>^>> MultiDocCache;
        static gcroot<System::Collections::Generic::List<SelectedObjectData^>^> CurrentSelectionCache;

        template <typename T, typename ParserFunc>
        static bool TryGetCachedValue(AcDbObjectId id, const std::wstring& propName, PropType expectedType, T& outValue, ParserFunc parser);

        static ArxBridge::Property^ FindPropertyInCache(AcDbObjectId id, const std::wstring& propName);

    public:
        static std::map<std::wstring, double> AggregatedDoubles;
        static std::map<std::wstring, int> AggregatedInts;

    public:
        static void Initialize();
        static void Terminate();

        static void ClearCache(AcDbDatabase* pDb);
        static void InvalidateObject(AcDbDatabase* pDb, AcDbObjectId id);
        static void PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds); 
        static void CommitSelectionChanges(AcDbDatabase* pDb);

        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outValue);
        static bool GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue);
                
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double newValue);
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int newValue);
        static bool SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, const std::wstring& newValue);

        static bool HasProperty(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName);
    };
}
