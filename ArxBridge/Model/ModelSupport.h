// Model/PropType.h
#pragma once

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
        property bool IsEnabled;

        // Конструктор со значениями по умолчанию
        PropertyMeta(System::String^ name, System::String^ description, ArxBridge::PropType type, bool isEnabled) {
            Name = name;
            Description = description;
            PropType = type;
            IsEnabled = isEnabled;
        }

        PropertyMeta(System::String^ name, System::String^ description, ArxBridge::PropType type) {
            Name = name;
            Description = description;
            PropType = type;
            IsEnabled = true; // По умолчанию свойство активно
        }
    };
} // namespace ArxBridge