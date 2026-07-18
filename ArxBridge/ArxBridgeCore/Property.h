#pragma once
#include "Enums.h" // Подключаем, чтобы видеть перечисление PropType

using namespace System;

namespace ArxBridge {

    // Предварительные объявления управляемых классов
    ref class PropertyMeta;
    ref class SelectedDataClass;

    /// <summary>
    /// Контейнер текущего состояния свойства объекта в кэше.
    /// Хранит строковые значения, тип данных и ссылку на родительский абстрактный класс.
    /// </summary>
    public ref class Property
    {
    private:
        // Уникальное имя-идентификатор свойства (ключ для связи с OPM и C# логикой)
        String^ _name;

        // Текущее строковое значение, считанное из чертежа
        String^ _value;

        // Новое строковое значение, введенное пользователем в стандартной панели свойств
        String^ _newValue;

        // Тип данных свойства (определяет маршалинг в COM VARIANT на стороне C++)
        ArxBridge::PropType _propType;

        // Флаг запрета редактирования свойства в OPM-панели
        bool _isReadOnly;

        // Флаг требования суммирования значений при множественном выборе объектов
        bool _isSum;

        // Флаг ошибки при получении значения
        bool _error; 
        // Ссылка на родительский абстрактный класс моста для доступа к динамическим параметрам
        SelectedDataClass^ _parent;

    public:
        // --- Конструкторы класса ---

        /// <summary> Конструктор по умолчанию. </summary>
        Property();

        /// <summary>
        /// Конструктор инициализации свойства через метаданные (паспорт свойства).
        /// </summary>
        Property(PropertyMeta^ meta, String^ value, bool isReadOnly, SelectedDataClass^ parent);

        // --- Методы жизненного цикла и сравнения ---

        /// <summary> 
        /// Покомпонентное сравнение текущего строкового значения с новым с учетом Precision из родителя.
        /// </summary>
        bool IsEqualValue(String^ value);

        // --- Управляемые свойства (Доступ к данным) ---

        /// <summary> Имя свойства. Служит ключом для поиска в пакетном кэше. </summary>
        property String^ Name {
            String^ get() { return _name; }
            void set(String^ value) { _name = value; }
        }

        /// <summary> Текущее валидное значение свойства в строковом виде. </summary>
        property String^ Value {
            String^ get() { return _value; }
            void set(String^ value) { _value = value; }
        }

        /// <summary> Новое строковое значение, ожидающее парсинга и фиксации в базе данных AutoCAD. </summary>
        property String^ NewValue {
            String^ get() { return _newValue; }
            void set(String^ value) { _newValue = value; }
        }

        /// <summary> Тип данных свойства для корректной упаковки в COM VARIANT. </summary>
        property ArxBridge::PropType PropType {
            ArxBridge::PropType get() { return _propType; }
            void set(ArxBridge::PropType value) { _propType = value; }
        }

        /// <summary> Флаг, указывающий, что строка значения была изменена пользователем в OPM. </summary>
        property bool IsChanged {
            bool get()
            {
                if (_newValue == nullptr || _value == nullptr) return false;
                return !IsEqualValue(_newValue);
            }
        }

        /// <summary> Определяет, заблокировано ли свойство для редактирования пользователем. </summary>
        property bool IsReadOnly {
            bool get() { return _isSum ? true : _isReadOnly; }
            void set(bool value) { _isReadOnly = value; }
        }

        /// <summary> Флаг требования суммирования значений при множественном выборе объектов. </summary>
        property bool IsSum {
            bool get() { return _isSum; }
            void set(bool value) { _isSum = value; }
        }
        /// <summary> 
        
        /// Флаг ошибки. Взводится C#-наследником, если свойства объекта повреждены, 
        /// не могут быть прочитаны или расчет параметра завершился сбоем.
        /// </summary>
        property bool Error {
            bool get() { return _error; }
            void set(bool value) { _error = value; }
        }

        /// <summary> Ссылка на родительский абстрактный класс моста. </summary>
        property SelectedDataClass^ Parent {
            SelectedDataClass^ get() { return _parent; }
            void set(SelectedDataClass^ value) { _parent = value; }
        }
    };
}
