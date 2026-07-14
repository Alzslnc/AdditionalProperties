#include "PropertyEngine.h"
#include "BridgeManager.h"
#include "../Model/Property.h"

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    // 1. ВЫДЕЛЕНИЕ ПАМЯТИ ПОД КОНТЕЙНЕРЫ КЭША
    std::map<AcDbDatabase*, gcroot<List<SelectedObjectData^>^>> PropertyEngine::MultiDocCache;
    gcroot<List<SelectedObjectData^>^> PropertyEngine::CurrentSelectionCache;

    std::map<std::wstring, double> PropertyEngine::AggregatedDoubles;
    std::map<std::wstring, int> PropertyEngine::AggregatedInts;

    // 2. ВСПОМОГАТЕЛЬНЫЙ ШАБЛОН ДЛЯ УСТРАНЕНИЯ ДУБЛИРОВАНИЯ КОДА (DRY)
    template <typename T, typename ParserFunc>
    bool PropertyEngine::TryGetCachedValue(AcDbObjectId id, const std::wstring& propName, PropType expectedType, T& outValue, ParserFunc parser) {

        // Безопасное приведение gcroot без трехэтажных скобок в стиле C
        if (static_cast<List<SelectedObjectData^>^>(PropertyEngine::CurrentSelectionCache) == nullptr) return false;

        List<SelectedObjectData^>^ currentSelection = PropertyEngine::CurrentSelectionCache;

        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || !objData->IsIdenticalTo(id) || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                Property^ prop = objData->Properties[j];
                if (prop == nullptr || prop->PropType != expectedType || prop->Value == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                if (std::wstring(wName) == propName) {
                    try {
                        pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                        outValue = parser(wVal); // Вызов лямбда-парсера
                        return true;
                    }
                    catch (...) {}
                }
            }
        }
        return false;
    }

    // 3. МЕТОДЫ ЖИЗНЕННОГО ЦИКЛА ЯДРА
    void PropertyEngine::Initialize() {
        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) == nullptr) {
            CurrentSelectionCache = gcnew List<SelectedObjectData^>();
        }
        AggregatedDoubles.clear();
        AggregatedInts.clear();
    }

    void PropertyEngine::Terminate() {
        MultiDocCache.clear();
        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) != nullptr) {
            CurrentSelectionCache->Clear();
        }
        AggregatedDoubles.clear();
        AggregatedInts.clear();
    }

    void PropertyEngine::ClearCache(AcDbDatabase* pDb) {
        if (pDb != nullptr && MultiDocCache.find(pDb) != MultiDocCache.end()) {
            MultiDocCache[pDb]->Clear();
            MultiDocCache.erase(pDb);
        }
    }

    void PropertyEngine::InvalidateObject(AcDbDatabase* pDb, AcDbObjectId id) {
        if (pDb == nullptr || !id.isValid() || MultiDocCache.find(pDb) == MultiDocCache.end()) return;

        List<SelectedObjectData^>^ dbCache = MultiDocCache[pDb];
        for (int i = 0; i < dbCache->Count; ++i) {
            if (dbCache[i]->IsIdenticalTo(id)) {
                dbCache[i]->IsModified = true;
                break;
            }
        }
    }

    // 4. ГЛАВНЫЙ КОНВЕЙЕР ОБРАБОТКИ ВЫБОРА
    void PropertyEngine::PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds) {
        if (pDb == nullptr || selectedIds.empty()) return;

        AggregatedDoubles.clear();
        AggregatedInts.clear();

        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) == nullptr) {
            CurrentSelectionCache = gcnew List<SelectedObjectData^>();
        }
        else {
            CurrentSelectionCache->Clear();
        }

        if (MultiDocCache.find(pDb) == MultiDocCache.end()) {
            MultiDocCache[pDb] = gcnew List<SelectedObjectData^>();
        }
        List<SelectedObjectData^>^ dbCache = MultiDocCache[pDb];
        List<SelectedObjectData^>^ nonActualList = gcnew List<SelectedObjectData^>();

        for (size_t i = 0; i < selectedIds.size(); ++i) {
            AcDbObjectId currentId = selectedIds[i];
            SelectedObjectData^ cachedItem = nullptr;

            for (int j = 0; j < dbCache->Count; ++j) {
                if (dbCache[j]->IsIdenticalTo(currentId)) {
                    cachedItem = dbCache[j];
                    break;
                }
            }

            if (cachedItem == nullptr) {
                cachedItem = gcnew SelectedObjectData(currentId);
                dbCache->Add(cachedItem);
            }

            CurrentSelectionCache->Add(cachedItem);

            if (cachedItem->IsModified) {
                if (cachedItem->Properties != nullptr) {
                    cachedItem->Properties->Clear();
                }
                else {
                    cachedItem->Properties = gcnew List<Property^>();
                }
                nonActualList->Add(cachedItem);
            }
        }

        List<SelectedObjectData^>^ currentSelection = CurrentSelectionCache;

        if (nonActualList->Count > 0) {
            BridgeManager::ExecuteBatchUpdate(nonActualList);
        }

        // Агрегация числовых сумм для КМ/КЖ
        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                Property^ prop = objData->Properties[j];
                if (prop == nullptr || !prop->IsSum || prop->Value == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                std::wstring nativeName(wName);

                pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                std::wstring nativeVal(wVal);

                try {
                    if (prop->PropType == PropType::IsDouble) {
                        AggregatedDoubles[nativeName] += std::stod(nativeVal);
                    }
                    else if (prop->PropType == PropType::IsInteger) {
                        AggregatedInts[nativeName] += std::stoi(nativeVal);
                    }
                }
                catch (...) {}
            }
        }
    }

    // 5. ЛАКОНИЧНЫЕ ПУБЛИЧНЫЕ ГЕТТЕРЫ НА ОСНОВЕ ШАБЛОНА
    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue) {
        auto itSum = AggregatedDoubles.find(propName);
        if (itSum != AggregatedDoubles.end()) {
            outValue = itSum->second;
            return true;
        }

        return TryGetCachedValue(id, propName, PropType::IsDouble, outValue, [](const wchar_t* str) {
            return std::stod(str);
            });
    }

    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue) {
        auto itSum = AggregatedInts.find(propName);
        if (itSum != AggregatedInts.end()) {
            outValue = itSum->second;
            return true;
        }

        return TryGetCachedValue(id, propName, PropType::IsInteger, outValue, [](const wchar_t* str) {
            return std::stoi(str);
            });
    }

    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue) {
        // Предполагается, что строки в вашем перечислении называются PropType::IsString
        return TryGetCachedValue(id, propName, PropType::IsString, outValue, [](const wchar_t* str) {
            return std::wstring(str);
            });
    }

} // namespace ArxBridge
