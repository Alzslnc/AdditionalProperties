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
    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outValue) {
        Property^ prop = FindPropertyInCache(id, propName);
        if (prop != nullptr) {
            outValue = prop->IsReadOnly; // Вызывается ваше C# свойство (которое учитывает IsSum)
            return true;
        }
        return false;
    }
    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue) {
        
        // 🚀 ИСПРАВЛЕНО: Если палитра запрашивает свойство, которое есть в сумматорах КМ/КЖ,
        // мы переводим число в строку и отдаем палитре!
        auto itDouble = AggregatedDoubles.find(propName);
        if (itDouble != AggregatedDoubles.end() && static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache)->Count > 1) {
            outValue = std::to_wstring(itDouble->second);
            // Красивый штрих: убираем лишние нули после запятой, если число целое (например, 45.000000 -> 45)
            outValue.erase(outValue.find_last_not_of(L'0') + 1, std::wstring::npos);
            if (outValue.back() == L'.') outValue.pop_back();
            return true;
        }

        auto itInt = AggregatedInts.find(propName);
        if (itInt != AggregatedInts.end() && static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache)->Count > 1) {
            outValue = std::to_wstring(itInt->second);
            return true;
        }

        // Если это не сумма, а обычное индивидуальное свойство объекта — вызываем наш шаблон поиска
        return TryGetCachedValue(id, propName, PropType::IsString, outValue, [](const wchar_t* str) {
            return std::wstring(str);
        });
    }

    ArxBridge::Property^ PropertyEngine::FindPropertyInCache(AcDbObjectId id, const std::wstring& propName) {
        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) == nullptr) return nullptr;

        List<SelectedObjectData^>^ currentSelection = CurrentSelectionCache;
        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || !objData->IsIdenticalTo(id) || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                Property^ prop = objData->Properties[j];
                if (prop == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                if (std::wstring(wName) == propName) {
                    return prop; // Нашли свойство, возвращаем его наружу!
                }
            }
        }
        return nullptr;
    }

    // 1. СЕТТЕР ДЛЯ ЧИСТЫХ ЧИСЕЛ С ПЛАВАЮЩЕЙ ТОЧКОЙ (DOUBLE)
    bool PropertyEngine::SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double newValue) {
        Property^ prop = FindPropertyInCache(id, propName);
        if (prop == nullptr || prop->IsReadOnly || prop->PropType != PropType::IsDouble) return false;

        prop->NewValue = newValue.ToString(System::Globalization::CultureInfo::InvariantCulture);
        return true;
    }

    // 2. СЕТТЕР ДЛЯ ЦЕЛИКОВЫХ ЧИСЕЛ (INT)
    bool PropertyEngine::SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int newValue) {
        Property^ prop = FindPropertyInCache(id, propName);
        if (prop == nullptr || prop->IsReadOnly || prop->PropType != PropType::IsInteger) return false;

        prop->NewValue = newValue.ToString();
        return true;
    }

    // 3. УНИВЕРСАЛЬНЫЙ СТРОКОВЫЙ СЕТТЕР ДЛЯ VT_BSTR (ТЕКУЩИЙ РАБОЧИЙ ВАРИАНТ)
    bool PropertyEngine::SetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, const std::wstring& newValueRaw) {
        Property^ prop = FindPropertyInCache(id, propName);
        if (prop == nullptr || prop->IsReadOnly) return false;

        // Нормализуем строку (заменяем запятую на точку для проверки)
        std::wstring normalizedStr = newValueRaw;
        size_t commaPos = normalizedStr.find(L',');
        if (commaPos != std::wstring::npos) {
            normalizedStr[commaPos] = L'.';
        }

        // Полноценная валидация типа данных
        try {
            if (prop->PropType == PropType::IsDouble) {
                size_t processedChars = 0;
                std::stod(normalizedStr, &processedChars);
                if (processedChars != normalizedStr.length()) return false;
            }
            else if (prop->PropType == PropType::IsInteger) {
                size_t processedChars = 0;
                std::stoi(normalizedStr, &processedChars);
                if (processedChars != normalizedStr.length()) return false;
            }
        }
        catch (...) {
            return false; // Ввели буквы в числовое поле
        }

        // Запись успешна
        prop->NewValue = gcnew System::String(newValueRaw.c_str());
        return true;
    }


    // 🚀 ВСПОМОГАТЕЛЬНЫЙ МЕТОД ФИЛЬТРАЦИИ ОБЪЕКТОВ ДЛЯ ПАЛИТРЫ
    bool PropertyEngine::HasProperty(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName) {
        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) == nullptr) return false;

        List<SelectedObjectData^>^ currentSelection = CurrentSelectionCache;

        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || !objData->IsIdenticalTo(id) || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                Property^ prop = objData->Properties[j];
                if (prop == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                if (std::wstring(wName) == propName) {
                    return true;
                }
            }
        }
        return false;
    }

    // 🚀 6. ГЛАВНЫЙ СИНХРОННЫЙ КОНВЕЙЕР ОБРАБОТКИ ЗАПИСИ
    void PropertyEngine::CommitSelectionChanges(AcDbDatabase* pDb) {
        if (pDb == nullptr) return;

        // Безопасное приведение нашего короткоживущего кэша текущего выбора
        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) == nullptr) return;

        List<SelectedObjectData^>^ currentSelection = CurrentSelectionCache;

        // Создаем отдельный изолированный список только для ИЗМЕНЕННЫХ объектов
        List<SelectedObjectData^>^ changedList = gcnew List<SelectedObjectData^>();

        // Собираем корзину измененных элементов на основе вашего свойства IsChanged
        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData != nullptr && objData->IsChanged) {
                changedList->Add(objData);
            }
        }

        // Если пользователь действительно что-то поменял — отправляем пакет на запись!
        if (changedList->Count > 0) {

            // Вызываем наш обновленный метод записи с флагами Locked Layer
            BridgeManager::ExecuteBatchWrite(changedList);

            // Финальный штрих: сбрасываемFlyweight-сумматоры КМ/КЖ, 
            // так как геометрия изменилась, и старые суммы больше не актуальны!
            AggregatedDoubles.clear();
            AggregatedInts.clear();
        }
    }

} // namespace ArxBridge
