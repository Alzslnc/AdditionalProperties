#pragma once
#include <map>
#include "dbid.h"
#include <vcclr.h>
#include <string>

namespace ArxBridge {

    // Опережающие объявления управляемых классов
    ref class SelectedObjectData;
    ref class Property;

    /// <summary>
    /// Низкоуровневый менеджер кэша документов AutoCAD.
    /// Отвечает исключительно за хранение объектов в ОЗУ, точечный сброс флагов и защиту от утечек памяти.
    /// </summary>
    class DocumentCacheManager
    {
    private:
        // Глобальная карта кэшей: База данных DWG -> Список закэшированных объектов .NET
        static std::map<AcDbDatabase*, gcroot<System::Collections::Generic::List<ArxBridge::SelectedObjectData^>^>> m_multiDocCache;

        // Временный кэш текущего активного набора выбора пользователя
        static gcroot<System::Collections::Generic::List<ArxBridge::SelectedObjectData^>^> m_currentSelectionCache;
               

        // Конструктор скрыт, так как класс статический
        DocumentCacheManager() = delete;

    public:       

        /// <summary> Инициализировать контейнеры кэша. </summary>
        static void Initialize();

        /// <summary> Полностью очистить контейнеры кэша при выгрузке плагина. </summary>
        static void Terminate();

        /// <summary> Уничтожить кэш конкретного чертежа при закрытии вкладки. </summary>
        static void ClearCache(AcDbDatabase* pDb);

        /// <summary> Точечно взвести флаг IsModified=true для измененного объекта. Вызывается реактором. </summary>
        static void InvalidateObject(AcDbDatabase* pDb, AcDbObjectId id);

        /// <summary> Найти элемент кэша объекта по его нативному ID в указанной базе данных. </summary>
        static ArxBridge::SelectedObjectData^ FindObjectInCache(AcDbDatabase* pDb, AcDbObjectId id);

        /// <summary> Найти конкретное свойство объекта в кэше. </summary>
        static ArxBridge::Property^ FindPropertyInCache(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName);

        /// <summary> Задать/обновить список объектов для текущего набора выбора чертежа. </summary>
        static void SetSelectionCache(AcDbDatabase* pDb, System::Collections::Generic::List<ArxBridge::SelectedObjectData^>^ newSelection);

        /// <summary> Получить текущий активный кэш выбора. </summary>
        static System::Collections::Generic::List<ArxBridge::SelectedObjectData^>^ GetCurrentSelectionCache();
    };
}
