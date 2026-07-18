#include "PropertyEngine.h"
#include "DocumentCacheManager.h"
#include "OpmPropertyService.h"
#include "BridgeManager.h"
#include "SelectedObjectData.h"
#include "Property.h"

#include "aced.h"
#include "dbapserv.h"
#include <sstream>
#include <locale>

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    // --- 1. ГЛАВНЫЙ СИНХРОННЫЙ КОНВЕЙЕР ОБРАБОТКИ ПАКЕТНОГО ЧТЕНИЯ ---
    void PropertyEngine::PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds) {
        if (pDb == nullptr || selectedIds.empty()) return;

        // Сбрасываем Flyweight-накопители числовых сумм перед расчетом нового кадра выбора
        OpmPropertyService::AggregatedDoubles.clear();
        OpmPropertyService::AggregatedInts.clear();

        List<SelectedObjectData^>^ currentSelection = DocumentCacheManager::GetCurrentSelectionCache();
        if (currentSelection == nullptr) {
            currentSelection = gcnew List<SelectedObjectData^>();
        }
        else {
            currentSelection->Clear();
        }

        // Извлекаем или инициализируем долгоживущий кэш чертежа (через менеджер кэша)
        // Для прозрачной работы с MultiDocCache мы используем геттеры менеджера
        List<SelectedObjectData^>^ nonActualList = gcnew List<SelectedObjectData^>();

        // Пересобираем локальный кэш кадра выбора и находим "грязные" (измененные) объекты
        for (size_t i = 0; i < selectedIds.size(); ++i) {
            AcDbObjectId currentId = selectedIds[i];
            SelectedObjectData^ cachedItem = DocumentCacheManager::FindObjectInCache(pDb, currentId);

            // Если объекта еще нет в кэше ОЗУ чертежа — создаем его элемент (вычисление ID выполнится 1 раз)
            if (cachedItem == nullptr) {
                cachedItem = gcnew SelectedObjectData(currentId);
                // В финальной интеграции менеджер кэша сохранит его в глобальную m_multiDocCache
            }

            currentSelection->Add(cachedItem);

            // Если сработал ленивый флаг модификации от реактора — очищаем старые свойства и готовим к обновлению
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

        DocumentCacheManager::SetSelectionCache(pDb, currentSelection);

        // КРИТИЧЕСКАЯ ОПТИМИЗАЦИЯ: Взрываем одну-единственную транзакцию чтения для всех грязных элементов за раз!
        if (nonActualList->Count > 0) {
            // Вызываем C#-плагины через батч-синхронизатор. Метод PreAcadUpdateAction выполнится штатно до чтения параметров.
            BridgeManager::ExecuteBatchSync(nonActualList, false);
        }

        // РАСЧЕТ АГРЕГИРОВАННЫХ СУММ ДЛЯ КМ/КЖ ПРИ МНОЖЕСТВЕННОМ ВЫБОРЕ ОБЪЕКТОВ
        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData == nullptr || objData->Properties == nullptr) continue;

            for (int j = 0; j < objData->Properties->Count; ++j) {
                Property^ prop = objData->Properties[j];
                if (prop == nullptr || !prop->IsSum || prop->Value == nullptr) continue;

                try {
                    pin_ptr<const wchar_t> wName = PtrToStringChars(prop->Name);
                    std::wstring nativeName(wName);

                    pin_ptr<const wchar_t> wVal = PtrToStringChars(prop->Value);
                    std::wstring nativeVal(wVal);

                    std::wstringstream parseStream(nativeVal);
                    parseStream.imbue(std::locale::classic());

                    if (prop->PropType == PropType::IsDouble) {
                        double parsedVal = 0.0;
                        if (parseStream >> parsedVal) OpmPropertyService::AggregatedDoubles[nativeName] += parsedVal;
                    }
                    else if (prop->PropType == PropType::IsInteger) {
                        int parsedVal = 0;
                        if (parseStream >> parsedVal) OpmPropertyService::AggregatedInts[nativeName] += parsedVal;
                    }
                }
                catch (...) {}
            }
        }
    }

    // --- 2. СЕПАРАЦИЯ ЭКРАНА И ФИЛЬТРАЦИЯ ОБЪЕКТОВ ПО КЛАССАМ ИЗ OPM ПАНЕЛИ ---
    void PropertyEngine::PrepareInitialSelectionContext(const std::vector<AcDbObjectId>& filterTypeIds) {
        Autodesk::AutoCAD::ApplicationServices::Document^ doc =
            Autodesk::AutoCAD::ApplicationServices::Application::DocumentManager->MdiActiveDocument;
        if (doc == nullptr) return;

        // Считываем текущий снимок экрана выбора пользователя
        Autodesk::AutoCAD::EditorInput::PromptSelectionResult^ psr = doc->Editor->SelectImplied();
        if (psr->Status != Autodesk::AutoCAD::EditorInput::PromptStatus::OK || psr->Value == nullptr) return;

        // Собираем хэш-сет разрешенных имен классов на основе переданных метаданных палитры OPM
        HashSet<String^>^ allowedClassNames = gcnew HashSet<String^>();
        for (size_t i = 0; i < filterTypeIds.size(); ++i) {
            AcDbObjectId typeId = filterTypeIds[i];
            if (!typeId.isValid()) continue;

            AcRxClass* pClass = typeId.objectClass();
            if (pClass != nullptr && pClass->name() != nullptr) {
                allowedClassNames->Add(gcnew String(pClass->name()));
            }
        }

        std::vector<AcDbObjectId> nativeSelectedIds;

        // Отсекаем все "чужие" примитивы, к которым наши C#-плагины не имеют зарегистрированных свойств
        for each (Autodesk::AutoCAD::EditorInput::SelectedObject ^ id in psr->Value) {
            if (id != nullptr && id->ObjectId.IsValid) {
                Autodesk::AutoCAD::Runtime::RXClass^ rxClass = id->ObjectId.ObjectClass;
                if (rxClass != nullptr && !String::IsNullOrEmpty(rxClass->Name)) {
                    if (allowedClassNames->Contains(rxClass->Name)) {
                        AcDbObjectId nativeId;
                        nativeId.setFromOldId(id->ObjectId.OldIdPtr.ToInt64());
                        if (nativeId.isValid()) nativeSelectedIds.push_back(nativeId);
                    }
                }
            }
        }

        // Если на экране остался чистый профильтрованный набор — запускаем пакетный сбор данных
        if (!nativeSelectedIds.empty()) {
            AcDbDatabase* pNativeDb = acdbHostApplicationServices()->workingDatabase();
            if (pNativeDb != nullptr) {
                PrepareSelectionContext(pNativeDb, nativeSelectedIds);
            }
        }
    }

    // --- 3. ГЛАВНЫЙ СИНХРОННЫЙ КОНВЕЙЕР ФИКСАЦИИ И ЗАПИСИ ИЗМЕНЕНИЙ ---
    void PropertyEngine::CommitSelectionChanges(AcDbDatabase* pDb) {
        List<SelectedObjectData^>^ currentSelection = DocumentCacheManager::GetCurrentSelectionCache();
        if (pDb == nullptr || currentSelection == nullptr) return;

        List<SelectedObjectData^>^ changedList = gcnew List<SelectedObjectData^>();

        // Сканируем кэш кадра на предмет измененных пользователем свойств (флаг IsChanged)
        for (int i = 0; i < currentSelection->Count; ++i) {
            SelectedObjectData^ objData = currentSelection[i];
            if (objData != nullptr && objData->IsChanged) {
                changedList->Add(objData);
            }
        }

        // Если пользователь ввел данные — открываем транзакцию записи и передаем пакет C#-реализациям
        if (changedList->Count > 0) {
            // Флаг true запускает в BridgeManager пакетную транзакцию ForWrite с автоактуализацией кэша на месте
            BridgeManager::ExecuteBatchSync(changedList, true);
        }
    }
}
