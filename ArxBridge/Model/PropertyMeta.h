#pragma once
#include "Enums.h"

namespace ArxBridge {

    /// <summary>
    /// Класс метаданных динамического свойства.
    /// Используется C#-плагинами для конфигурации и регистрации своих свойств в нативном OPM-мосте.
    /// Не хранит значения объектов чертежа, описывает только "паспорт" свойства.
    /// </summary>
    public ref class PropertyMeta
    {
    public:
        /// <summary> Отображаемое имя свойства в панели AutoCAD (например, "Длина дуги"). </summary>
        property System::String^ Name;

        /// <summary> Описание свойства. Отображается в нижней всплывающей подсказке панели OPM при выборе свойства. </summary>
        property System::String^ Description;

        /// <summary> Тип данных свойства, определяющий его маршалинг в COM/VARIANT на стороне C++. </summary>
        property ArxBridge::PropType PropType;

        /// <summary> 
        /// Список типов объектов AutoCAD API (например, typeof(Line)), к которым применимо данное свойство.
        /// Если список пуст, мост может трактовать свойство как глобальное для всех типов сущностей.
        /// </summary>
        property System::Collections::Generic::List<System::Type^>^ Types;

        /// <summary> 
        /// Флаг глобальной активности свойства. 
        /// Управляется из внешнего WPF-окна настроек. Если false — свойство полностью игнорируется 
        /// нативным мостом при загрузке плагина и не регистрируется в OPM-менеджере AutoCAD.
        /// </summary>
        property bool IsEnabled;

        /// <summary> Тип стандартной группы AutoCAD, в которой будет находиться свойство. </summary>
        property PropertyGroup GroupType;

        /// <summary> Имя кастомной группы. Используется только в случае, если GroupType == PropertyGroup::Custom. </summary>
        property System::String^ CustomGroupName;

        /// <summary>
        /// Конструктор для инициализации базового паспорта динамического свойства.
        /// </summary>
        /// <param name="name">Отображаемое имя свойства в интерфейсе.</param>
        /// <param name="description">Всплывающая подсказка для пользователя.</param>
        /// <param name="type">Тип данных (Строка, Число и т.д.).</param>
        PropertyMeta(System::String^ name, System::String^ description, ArxBridge::PropType type)
        {
            // Инициализируем список типов, чтобы избежать NullReferenceException на стороне C#
            Types = gcnew System::Collections::Generic::List<System::Type^>();

            Name = name;
            Description = description;
            PropType = type;

            // По умолчанию свойство включено и готово к регистрации
            IsEnabled = true;

            // По умолчанию относим свойство в стандартную категорию "Общие"
            GroupType = PropertyGroup::General;
            CustomGroupName = nullptr;
        }
    };

} // namespace ArxBridge
