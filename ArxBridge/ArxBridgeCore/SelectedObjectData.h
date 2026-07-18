#pragma once
#include "dbid.h" // Обязательный нативный заголовок AutoCAD для AcDbObjectId

namespace ArxBridge {

    ref class Property;

    /// <summary>
    /// Элемент пакетного кэша чертежа, представляющий конкретный объект AutoCAD.
    /// Хранит предвычисленные нативные и управляемые идентификаторы для работы без накладных расходов.
    /// </summary>
    public ref class SelectedObjectData
    {
    private:
        // Готовая управляемая структура .NET ObjectId. Хранится напрямую в памяти класса.
        Autodesk::AutoCAD::DatabaseServices::ObjectId _managedId;

        // Числовой 64-битный ID для мгновенного извлечения AcDbObjectId и сравнений в C++
        Adesk::IntDbId m_numericId;

        // Внутренние поля управляемых свойств
        System::Collections::Generic::List<ArxBridge::Property^>^ _properties;
        bool _isModified;

    public:
        // --- Конструкторы класса (Только объявления) ---

        /// <summary> Инициализирует элемент кэша и один раз кэширует все виды идентификаторов. </summary>
        SelectedObjectData(AcDbObjectId id);

        // --- ДЛЯ C# МОДУЛЯ (Управляемый интерфейс) ---

        /// <summary> Мгновенно возвращает готовую структуру .NET ObjectId из памяти без вычислений. </summary>
        property Autodesk::AutoCAD::DatabaseServices::ObjectId ObjectId {
            Autodesk::AutoCAD::DatabaseServices::ObjectId get() { return _managedId; }
        }

        /// <summary> Список закэшированных динамических свойств этого объекта. </summary>
        property System::Collections::Generic::List<ArxBridge::Property^>^ Properties {
            System::Collections::Generic::List<ArxBridge::Property^>^ get() { return _properties; }
            void set(System::Collections::Generic::List<ArxBridge::Property^>^ value) { _properties = value; }
        }

        /// <summary> Флаг ленивой актуализации (объект изменен в чертеже, нужно перечитать свойства). </summary>
        property bool IsModified {
            bool get() { return _isModified; }
            void set(bool value) { _isModified = value; }
        }

        /// <summary> Динамический флаг изменений, внесенных пользователем в OPM панели. </summary>
        property bool IsChanged {
            bool get();
        }

        // --- ДЛЯ C++ МОДУЛЯ (Нативный интерфейс идентификации) ---

        /// <summary> Метод мгновенного получения нативного AcDbObjectId из предвычисленного числа. </summary>
        AcDbObjectId GetNativeId();

        /// <summary> Прямое сравнение переданного ID с закэшированным числовым значением. </summary>
        bool IsIdenticalTo(AcDbObjectId id);
    };
}
