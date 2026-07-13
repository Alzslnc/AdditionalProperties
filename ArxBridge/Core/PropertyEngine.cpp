#include "PropertyEngine.h"
#include "BridgeManager.h"
#include "../Model/Property.h"

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    // Выделение памяти под долгоживущие и короткоживущие контейнеры ядра
    std::map<AcDbDatabase*, gcroot<List<ArxBridge::SelectedObjectData^>^>> PropertyEngine::MultiDocCache;
    gcroot<List<ArxBridge::SelectedObjectData^>^> PropertyEngine::CurrentSelectionCache;

    // Выделение памяти под Flyweight-сумматоры КМ/КЖ
    std::map<std::wstring, double> PropertyEngine::AggregatedDoubles;
    std::map<std::wstring, int> PropertyEngine::AggregatedInts;

    void PropertyEngine::Initialize() {
        if (((List<ArxBridge::SelectedObjectData^>^)CurrentSelectionCache) == nullptr) {
            CurrentSelectionCache = gcnew List<ArxBridge::SelectedObjectData^>();
        }
        AggregatedDoubles.clear();
        AggregatedInts.clear();
    }

    void PropertyEngine::Terminate() {
        MultiDocCache.clear();
        if (((List<ArxBridge::SelectedObjectData^>^)CurrentSelectionCache) != nullptr) {
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

        List<ArxBridge::SelectedObjectData^>^ dbCache = MultiDocCache[pDb];
        for (int i = 0; i < dbCache->Count; ++i) {
            // Высокоскоростное нативное сравнение указателей напрямую в ОЗУ чертежа
            if (dbCache[i]->IsIdenticalTo(id)) {
                dbCache[i]->IsModified = true; // Взводим флаг неактуальности для BridgeManager
                break;
            }
        }
    }
    void PropertyEngine::PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds) {
        if (pDb == nullptr || selectedIds.empty()) return;

        // Сбрасываем короткоживущие сумматоры текущего выбора
        AggregatedDoubles.clear();
        AggregatedInts.clear();
        if (((List<ArxBridge::SelectedObjectData^>^)CurrentSelectionCache) == nullptr) {
            CurrentSelectionCache = gcnew List<ArxBridge::SelectedObjectData^>();
        }
        else {
            CurrentSelectionCache->Clear();
        }

        // Поднимаем долгоживущий кэш для текущего .dwg файла
        if (MultiDocCache.find(pDb) == MultiDocCache.end()) {
            MultiDocCache[pDb] = gcnew List<ArxBridge::SelectedObjectData^>();
        }
        List<ArxBridge::SelectedObjectData^>^ dbCache = MultiDocCache[pDb];

        List<ArxBridge::SelectedObjectData^>^ nonActualList = gcnew List<ArxBridge::SelectedObjectData^>();

        // 1. ДИФФЕРЕНЦИАЛЬНЫЙ АНАЛИЗ КЭША ОБЪЕКТОВ
        for (size_t i = 0; i < selectedIds.size(); ++i) {
            AcDbObjectId currentId = selectedIds[i];
            ArxBridge::SelectedObjectData^ cachedItem = nullptr;

            for (int j = 0; j < dbCache->Count; ++j) {
                if (dbCache[j]->IsIdenticalTo(currentId)) {
                    cachedItem = dbCache[j];
                    break;
                }
            }

            // Если примитив выделен впервые — вызываем ваш новый нативный конструктор с AcDbObjectId
            if (cachedItem == nullptr) {
                cachedItem = gcnew ArxBridge::SelectedObjectData(currentId);
                dbCache->Add(cachedItem);
            }

            CurrentSelectionCache->Add(cachedItem);

            // ОЧИСТКА ВЫНЕСЕНА СЮДА: Чистим свойства перед пакетной транзакцией
            if (cachedItem->IsModified) {
                if (cachedItem->Properties != nullptr) {
                    cachedItem->Properties->Clear();
                }
                else {
                    cachedItem->Properties = gcnew List<ArxBridge::Property^>();
                }
                nonActualList->Add(cachedItem);
            }
        }

        // 2. ОДИН ПАКЕТНЫЙ ДЕСАНТ НА ВСЕ НЕАКТУАЛЬНЫЕ ОБЪЕКТЫ
        if (nonActualList->Count > 0) {
            BridgeManager::ExecuteBatchUpdate(nonActualList);
        }

        // 1. Извлекаем чистый управляемый список из нативной обертки gcroot [INDEX]
        List<ArxBridge::SelectedObjectData^>^ currentSelection = CurrentSelectionCache;


        // 3. ДИНАМИЧЕСКИЙ СБОР ПОЛУЧЕННЫХ ДАННЫХ И ПОДСЧЕТ СУММ КМ/КЖ
        for (int i = 0; i < currentSelection->Count; ++i) {
            ArxBridge::SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                ArxBridge::Property^ prop = objData->Properties[j];
                if (prop == nullptr || !prop->IsSum || prop->Value == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                std::wstring nativeName(wName);

                pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                std::wstring nativeVal(wVal);

                try {
                    if (prop->PropType == ArxBridge::PropType::IsDouble) {
                        AggregatedDoubles[nativeName] += std::stod(nativeVal);
                    }
                    else if (prop->PropType == ArxBridge::PropType::IsInteger) {
                        AggregatedInts[nativeName] += std::stoi(nativeVal);
                    }
                }
                catch (...) {}
            }
        }
    }
    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue) {
        auto itSum = AggregatedDoubles.find(propName);
        if (itSum != AggregatedDoubles.end()) {
            outValue = itSum->second; // Отдаем готовую сумму
            return true;
        }

        if (((List<ArxBridge::SelectedObjectData^>^)CurrentSelectionCache) == nullptr) return false;

        List<ArxBridge::SelectedObjectData^>^ currentSelection = CurrentSelectionCache;

        for (int i = 0; i < currentSelection->Count; ++i) {
            ArxBridge::SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || !objData->IsIdenticalTo(id) || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                ArxBridge::Property^ prop = objData->Properties[j];
                if (prop == nullptr || prop->PropType != ArxBridge::PropType::IsDouble || prop->Value == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                if (std::wstring(wName) == propName) {
                    try {
                        // ИСПРАВЛЕНО: Закрепляем управляемую строку в памяти перед парсингом!
                        pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                        outValue = std::stod(wVal); // Теперь это чистый const wchar_t*
                        return true;
                    }
                    catch (...) {}
                }
                            }
        }
        return false;
    }

    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue) {
        auto itSum = AggregatedInts.find(propName);
        if (itSum != AggregatedInts.end()) {
            outValue = itSum->second; // Отдаем готовую сумму
            return true;
        }

        if (((List<ArxBridge::SelectedObjectData^>^)CurrentSelectionCache) == nullptr) return false;

        List<ArxBridge::SelectedObjectData^>^ currentSelection = CurrentSelectionCache;

        for (int i = 0; i < currentSelection->Count; ++i) {
            ArxBridge::SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || !objData->IsIdenticalTo(id) || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                ArxBridge::Property^ prop = objData->Properties[j];
                if (prop == nullptr || prop->PropType != ArxBridge::PropType::IsInteger || prop->Value == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                if (std::wstring(wName) == propName) {
                    try {
                        // ИСПРАВЛЕНО: Закрепляем строку в памяти для std::stoi
                        pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                        outValue = std::stoi(wVal);
                        return true;
                    }
                    catch (...) {}
                }
            }
        }
        return false;
    }

    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue) {
     
        if (((List<ArxBridge::SelectedObjectData^>^)CurrentSelectionCache) == nullptr) return false;

        List<ArxBridge::SelectedObjectData^>^ currentSelection = CurrentSelectionCache;

        for (int i = 0; i < currentSelection->Count; ++i) {
            ArxBridge::SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || !objData->IsIdenticalTo(id) || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                ArxBridge::Property^ prop = objData->Properties[j];
                if (prop == nullptr || prop->Value == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                if (std::wstring(wName) == propName) {
                    pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                    outValue = std::wstring(wVal);
                    return true;
                }
            }
        }
        return false;
    }
} // Конец пространства имен ArxBridge
