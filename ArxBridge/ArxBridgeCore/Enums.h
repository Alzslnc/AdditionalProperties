#pragma once

namespace ArxBridge {

    /// <summary>
    /// Перечисление поддерживаемых типов данных для стандартной панели свойств AutoCAD (OPM).
    /// Определяет, как именно свойство будет отображаться и редактироваться пользователем.
    /// </summary>
    public enum class PropType
    {
        /// <summary> Текстовое поле ввода. В COM-модели преобразуется в BSTR/VT_BSTR. </summary>
        IsString,

        /// <summary> Число с плавающей точкой. Преобразуется в double/VT_R8. Подходит для координат, длин и т.д. </summary>
        IsDouble,

        /// <summary> Целое число. Преобразуется в long/VT_I4. Подходит для счетчиков, индексов и цветов. </summary>
        IsInteger,

        /// <summary> Логический тип (Да/Нет). В OPM панели отображается как выпадающий список True/False. </summary>
        IsBoolean,

        /// <summary> Выпадающий список с фиксированным набором значений. Требует дополнительной реализации списков. </summary>
        IsComboBox
    };

    /// <summary>
    /// Идентификаторы групп (категорий) в панели свойств AutoCAD.
    /// Значения соответствуют стандартным OPM-категориям ObjectARX (IPropertyManager2).
    /// </summary>
    public enum class PropertyGroup
    {
        /// <summary> Стандартная категория "Общие" (Цвет, Слой, Тип линий и т.д.). Идентификатор OPM: -3 </summary>
        General = -3,

        /// <summary> Стандартная категория "Геометрия" / "Положение". Идентификатор OPM: -4 </summary>
        Position = -4,

        /// <summary> Стандартная категория "3D-визуализация" (Материал, Область). Идентификатор OPM: -5 </summary>
        Visualization = -5,

        /// <summary> Кастомная пользовательская категория. Если выбран этот тип, имя группы берется из строки. </summary>
        Custom = 0
    };

} // namespace ArxBridge
