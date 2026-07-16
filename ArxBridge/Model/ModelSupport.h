#pragma once
#include "dbid.h" // Обязательный заголовок AutoCAD для AcDbObjectId

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


    public ref class PropertyMeta
    {
    public:
        property System::String^ Name;
        property System::String^ Description;
        property ArxBridge::PropType PropType;
        property System::Collections::Generic::List<System::Type^>^ Types;
        property bool IsEnabled;

        // Конструктор со значениями по умолчанию
        PropertyMeta(System::String^ name, System::String^ description, ArxBridge::PropType type, bool isEnabled) {
            Types = gcnew System::Collections::Generic::List<System::Type^>();
            Name = name;
            Description = description;
            PropType = type;
            IsEnabled = isEnabled;
        }

        PropertyMeta(System::String^ name, System::String^ description, ArxBridge::PropType type) {
            Types = gcnew System::Collections::Generic::List<System::Type^>();
            Name = name;
            Description = description;
            PropType = type;
            IsEnabled = true; // По умолчанию свойство активно
        }
    };
} // namespace ArxBridge