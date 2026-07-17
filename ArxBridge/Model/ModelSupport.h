#pragma once
//#include "dbid.h" // Обязательный заголовок AutoCAD для AcDbObjectId

namespace ArxBridge {

    /// <summary>
    /// Типы данных для встроенной панели AutoCAD
    /// </summary>
    public enum class PropType
    {
        IsString,
        IsDouble,
        IsInteger,
        IsBoolean,
        IsComboBox
    };

    public enum class PropertyGroup
    {
        General = -3,         // Общие
        Position = -4,        // Положение
        Visualization = -5,   // 3D-визуализация
        Custom = 0            // Кастомная группа (имя задается строкой)
    };

    public ref class PropertyMeta
    {
    public:
        property System::String^ Name;
        property System::String^ Description;
        property ArxBridge::PropType PropType;
        property System::Collections::Generic::List<System::Type^>^ Types;
        property bool IsEnabled;

        // 🚀 НОВЫЕ БЕЗОПАСНЫЕ ПОЛЯ ДЛЯ РАЗРАБОТЧИКА
        property PropertyGroup GroupType;       // Тип группы (Enum)
        property System::String^ CustomGroupName; // Имя кастомной группы (если GroupType == Custom)

        // В конструкторах по умолчанию задаем базовую категорию:
        PropertyMeta(System::String^ name, System::String^ description, ArxBridge::PropType type) {
            Types = gcnew System::Collections::Generic::List<System::Type^>();
            Name = name;
            Description = description;
            PropType = type;
            IsEnabled = true;

            // По умолчанию кладем в Общие
            GroupType = PropertyGroup::General;
            CustomGroupName = nullptr;
        }
        // ... аналогично для второго конструктора ...
    };
} // namespace ArxBridge