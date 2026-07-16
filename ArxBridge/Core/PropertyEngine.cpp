#include "aced.h"      // Для acedSSGet, acedSSLength, acedSSFree
#include "dbid.h"      // Для AcDbObjectId
#include "dbapserv.h"  // Для acdbHostApplicationServices

#include "PropertyEngine.h"
#include "BridgeManager.h"
#include "../Model/Property.h"
#include "../Core/ArxBridgeCore.h"

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    // 1. ВЫДЕЛЕНИЕ ПАМЯТИ ПОД КОНТЕЙНЕРЫ КЭША
    std::map<AcDbDatabase*, gcroot<List<SelectedObjectData^>^>> PropertyEngine::MultiDocCache;
    gcroot<List<SelectedObjectData^>^> PropertyEngine::CurrentSelectionCache;

    std::map<std::wstring, double> PropertyEngine::AggregatedDoubles;
    std::map<std::wstring, int> PropertyEngine::AggregatedInts;

    // Инициализация при старте плагина
    void PropertyEngine::Initialize() {
        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) == nullptr) {
            CurrentSelectionCache = gcnew List<SelectedObjectData^>();
        }
        AggregatedDoubles.clear();
        AggregatedInts.clear();
    }

    // Выгрузка плагина / очистка памяти
    void PropertyEngine::Terminate() {
        MultiDocCache.clear();
        if (static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) != nullptr) {
            CurrentSelectionCache->Clear();
        }
        AggregatedDoubles.clear();
        AggregatedInts.clear();
    }

    // Полная зачистка кэша конкретного документа при его закрытии
    void PropertyEngine::ClearCache(AcDbDatabase* pDb) {
        if (pDb != nullptr && MultiDocCache.find(pDb) != MultiDocCache.end()) {
            MultiDocCache[pDb]->Clear();
            MultiDocCache.erase(pDb);
        }
    }

    // Сброс флага актуальности объекта (вызывается нативными реакторами при изменении геометрии ручками)
    void PropertyEngine::InvalidateObject(AcDbDatabase* pDb, AcDbObjectId id) {
        if (pDb == nullptr || !id.isValid() || MultiDocCache.find(pDb) == MultiDocCache.end()) return;

        List<SelectedObjectData^>^ dbCache = MultiDocCache[pDb];
        for (int i = 0; i < dbCache->Count; ++i) {
            if (dbCache[i]->IsIdenticalTo(id)) {
                dbCache[i]->IsModified = true; // Сигнал для ExecuteBatchUpdate на следующем кадре!
                break;
            }
        }
    }
    // 2. ВСПОМОГАТЕЛЬНЫЙ ШАБЛОН ДЛЯ УСТРАНЕНИЯ ДУБЛИРОВАНИЯ КОДА (DRY)
    // 🚀 ИСПРАВЛЕНО: Полностью очищен от привязки к expectedType, работает чисто по именам строк!
    template <typename T, typename ParserFunc>
    bool PropertyEngine::TryGetCachedValue(AcDbObjectId id, const std::wstring& propName, T& outValue, ParserFunc parser) {

        if (static_cast<List<SelectedObjectData^>^>(PropertyEngine::CurrentSelectionCache) == nullptr) return false;
        List<SelectedObjectData^>^ currentSelection = PropertyEngine::CurrentSelectionCache;

        // Поиск по локальному кэшу текущего кадра (Максимальная скорость O(1))
        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || !objData->IsIdenticalTo(id) || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                Property^ prop = objData->Properties[j];
                if (prop == nullptr || prop->Value == nullptr) continue;

                pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                if (std::wstring(wName) == propName) {
                    try {
                        pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                        outValue = parser(wVal); // Лямбда-парсер на месте сделает stod, stoi или std::wstring
                        return true;
                    }
                    catch (...) {}
                }
            }
        }
        return false;
    }
    // 4. ГЛАВНЫЙ КОНВЕЙЕР ОБРАБОТКИ ВЫБОРА
    void PropertyEngine::PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds) {
        if (pDb == nullptr || selectedIds.empty()) return;

        // Сбрасываем старые накопители сумм множественного выбора
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

        // Склеиваем нативный вектор выделения с кэшем оперативной памяти чертежа
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

            // Наполняем локальный кэш текущего кадра палитры свойств
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

        // ВЗРЫВАЕМ ОДНУ ЕДИНСТВЕННУЮ ТРАНЗАКЦИЮ НА ЧТЕНИЕ ИЗМЕНЕННЫХ ЭЛЕМЕНТОВ!
        if (nonActualList->Count > 0) {
            BridgeManager::ExecuteBatchSync(nonActualList, false); // false = чистый режим чтения
        }

        // РАСЧЕТ АГРЕГИРОВАННЫХ СУММ ДЛЯ КМ/КЖ ПРИ МНОЖЕСТВЕННОМ ВЫБОРЕ
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
    // 🚀 ОБНОВЛЕННЫЙ ОРКЕСТРАТОР С ПОДДЕРЖКОЙ ФИЛЬТРОВ ПАНЕЛИ OPM
    void PropertyEngine::PrepareInitialSelectionContext(const std::vector<AcDbObjectId>& filterTypeIds)
    {
        Autodesk::AutoCAD::ApplicationServices::Document^ doc =
            Autodesk::AutoCAD::ApplicationServices::Application::DocumentManager->MdiActiveDocument;

        if (doc == nullptr) return;

        // Считываем грязный снимок экрана (там лежат и линии, и круги вместе)
        Autodesk::AutoCAD::EditorInput::PromptSelectionResult^ psr = doc->Editor->SelectImplied();
        if (psr->Status != Autodesk::AutoCAD::EditorInput::PromptStatus::OK || psr->Value == nullptr) return;

        // 🎯 1. СОБИРАЕМ СПИСОК РАЗРЕШЕННЫХ КЛАССОВ ИЗ НАШЕГО МИКРО-НАКОПИТЕЛЯ
        System::Collections::Generic::HashSet<System::String^>^ allowedClassNames =
            gcnew System::Collections::Generic::HashSet<System::String^>();

        for (size_t i = 0; i < filterTypeIds.size(); ++i) {
            AcDbObjectId typeId = filterTypeIds[i];
            if (!typeId.isValid()) continue;

            // Магия ObjectARX: заглядываем в дескриптор нативного класса объекта
            AcRxClass* pClass = typeId.objectClass();
            if (pClass != nullptr && pClass->name() != nullptr) {
                // Переводим нативное имя класса (например, L"AcDbCircle") в строку .NET
                System::String^ managedClassName = gcnew System::String(pClass->name());
                allowedClassNames->Add(managedClassName);
            }
        }

        // 🎯 2. СЕПАРАЦИЯ ЭКРАНА: Фильтруем грязный SelectImplied() строго по разрешенным типам!
        std::vector<AcDbObjectId> nativeSelectedIds;

        for each (Autodesk::AutoCAD::EditorInput::SelectedObject ^ id in psr->Value)
        {
            if (id != nullptr && id->ObjectId.IsValid)
            {
                // Достаем имя нативного класса из объекта .NET
                Autodesk::AutoCAD::Runtime::RXClass^ rxClass = id->ObjectId.ObjectClass;

                if (rxClass != nullptr && !System::String::IsNullOrEmpty(rxClass->Name))
                {
                    // Если тип объекта (например, линия) отсутствует в списке разрешенных типов фильтра палитры — 
                    // мы его полностью ИГНОРИРУЕМ и не пускаем в транзакции чтения и сумматоры!
                    if (allowedClassNames->Contains(rxClass->Name))
                    {
                        AcDbObjectId nativeId;
                        nativeId.setFromOldId(id->ObjectId.OldIdPtr.ToInt64());

                        if (nativeId.isValid()) {
                            nativeSelectedIds.push_back(nativeId);
                        }
                    }
                }
            }
        }

        // Передаем идеально чистый, соответствующий фильтрам OPM вектор нашему DRY-обходчику!
        if (!nativeSelectedIds.empty()) {
            AcDbDatabase* pNativeDb = acdbHostApplicationServices()->workingDatabase();
            if (pNativeDb != nullptr) {
                PrepareSelectionContext(pNativeDb, nativeSelectedIds);
            }
        }
    }
    // 5. ЛАКОНИЧНЫЕ ПУБЛИЧНЫЕ ГЕТТЕРЫ НА ОСНОВЕ ШАБЛОНА
    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, double& outValue) {

        // Если это агрегированная сумма — сразу отдаем её из мапы
        auto itSum = AggregatedDoubles.find(propName);
        if (itSum != AggregatedDoubles.end()) {
            outValue = itSum->second;
            return true;
        }

        // 🎯 СЧИТЫВАНИЕ ТОЧНОСТИ ИЗ C# РОДИТЕЛЯ НАБОРА
        int displayPrecision = 6;
        Property^ propInCache = FindPropertyInCache(id, propName);
        if (propInCache != nullptr && propInCache->Parent != nullptr) {
            try { propInCache->Parent->PreAcadUpdateAction(); }
            catch (...) {}
            displayPrecision = propInCache->Parent->Precision;
        }

        // Вычитываем сырое число из кэша через наш универсальный шаблон
        double rawDoubleValue = 0.0;
        if (TryGetCachedValue(id, propName, rawDoubleValue, [](const wchar_t* str) { return std::stod(str); })) {

            // 🚀 МАТЕМАТИЧЕСКОЕ ОКРУГЛЕНИЕ ДЛЯ DOUBLE:
            // Чтобы два double-значения разных линий не отличались на мусорные микроны IEEE 754,
            // мы принудительно округляем сам double до displayPrecision знаков после запятой!
            double factor = std::pow(10.0, displayPrecision);
            outValue = std::round(rawDoubleValue * factor) / factor;

            return true;
        }

        return false;
    }


    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, int& outValue) {
        auto itSum = AggregatedInts.find(propName);
        if (itSum != AggregatedInts.end()) {
            outValue = itSum->second;
            return true;
        }

        // 🚀 ИСПРАВЛЕНО ДЛЯ C2660: Убран аргумент типа PropType::IsInteger, передаем только имя и лямбду!
        return TryGetCachedValue(id, propName, outValue, [](const wchar_t* str) {
            return std::stoi(str);
            });
    }

    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, bool& outValue) {
        Property^ prop = FindPropertyInCache(id, propName);
        if (prop != nullptr) {
            outValue = prop->IsReadOnly;
            return true;
        }
        return false;
    }

    bool PropertyEngine::GetPropertyValue(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, std::wstring& outValue) {

        // 1. Проверяем, множественный ли выбор сейчас на палитре
        bool isMultiSelection = (ArxBridgeCore::m_frameSelectedIds != nullptr && ArxBridgeCore::m_frameSelectedIds->size() > 1);

        // 🧮 2. ЕСЛИ МНОЖЕСТВЕННЫЙ ВЫБОР: Выдаем агрегированные суммы КМ/КЖ
        if (isMultiSelection) {
            // Ищем в double-сумматорах
            auto itDouble = AggregatedDoubles.find(propName);
            if (itDouble != AggregatedDoubles.end()) {
                int displayPrecision = 6; // Дефолт
                Property^ propInCache = FindPropertyInCache(id, propName);
                if (propInCache != nullptr && propInCache->Parent != nullptr) {
                    displayPrecision = propInCache->Parent->Precision;
                }

                std::wstringstream wss;
                wss << std::fixed << std::setprecision(displayPrecision) << itDouble->second;
                outValue = wss.str();

                if (displayPrecision > 0) {
                    outValue.erase(outValue.find_last_not_of(L'0') + 1, std::wstring::npos);
                    if (outValue.back() == L'.') outValue.pop_back();
                }
                return true;
            }

            // Ищем в int-сумматорах (для штук, количеств)
            auto itInt = AggregatedInts.find(propName);
            if (itInt != AggregatedInts.end()) {
                outValue = std::to_wstring(itInt->second);
                return true;
            }
        }

        // 🏢 3. ЕСЛИ ОДИНОЧНЫЙ ВЫБОР (или обычное текстовое свойство вроде Марки):
        // Просто забираем готовую, идеальную строку из кэша, которую нам выдал C# провайдер!
        // Никаких std::stod и переформатирований здесь делать не нужно.
        return TryGetCachedValue(id, propName, outValue, [](const wchar_t* str) {
            return std::wstring(str);
            });
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

        std::wstring normalizedStr = newValueRaw;
        size_t commaPos = normalizedStr.find(L',');
        if (commaPos != std::wstring::npos) {
            normalizedStr[commaPos] = L'.';
        }

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
        catch (...) { return false; }

        prop->NewValue = gcnew System::String(newValueRaw.c_str());
        return true;
    }

    // Вспомогательный метод поиска свойства в ОЗУ
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
                if (std::wstring(wName) == propName) return prop;
            }
        }
        return nullptr;
    }

    bool PropertyEngine::HasProperty(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName) {
        return (FindPropertyInCache(id, propName) != nullptr);
    }

    bool PropertyEngine::GetPropertyType(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName, PropType& outType) {
        Property^ prop = FindPropertyInCache(id, propName);
        if (prop != nullptr) {
            outType = prop->PropType;
            return true;
        }
        return false;
    }

    // 🚀 6. ГЛАВНЫЙ СИНХРОННЫЙ КОНВЕЙЕР ОБРАБОТКИ ЗАПИСИ
    void PropertyEngine::CommitSelectionChanges(AcDbDatabase* pDb) {
        if (pDb == nullptr || static_cast<List<SelectedObjectData^>^>(CurrentSelectionCache) == nullptr) return;

        List<SelectedObjectData^>^ currentSelection = CurrentSelectionCache;
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
            BridgeManager::ExecuteBatchSync(changedList, true); // true = запись + мгновенное чтение на месте
            AggregatedDoubles.clear();
            AggregatedInts.clear();
        }
    }

} // namespace ArxBridge
