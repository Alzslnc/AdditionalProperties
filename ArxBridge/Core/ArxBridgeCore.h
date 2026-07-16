#pragma once
#include <vector>
#include <Unknwn.h> 
#include "Model/ModelSupport.h"

namespace ArxBridge {

    /// <summary>
    /// Состояния конечного автомата пакетного OPM-конвейера палитры AutoCAD.
    /// Ультра-минималистичный вариант после удаления фазы поштучного сбора ID.
    /// </summary>
    public enum class BridgeState
    {
        /// <summary>
        /// Исходное состояние ожидания начала нового кадра палитры.
        /// Выставляется на входе в GetDisplayName. Сигнализирует о готовности 
        /// запустить пакетную транзакцию чтения при первом же удобном такте интерфейса.
        /// </summary>
        Read,

        /// <summary>
        /// Жесткий замок (блокировка) кадра.
        /// Взводится строго один раз на кадр (в Connect для палитры или в GetValue для тултипа).
        /// Запрещает лавинные повторные транзакции O(N²) и гоняет чтение по готовому C# кэшу.
        /// </summary>
        Locked,

        /// <summary>
        /// Фаза фиксации изменений (Commit).
        /// Взводится методом SetCurrentValueData при вводе пользователем новых данных в ячейку.
        /// Сигнализирует следующему такту GetValue, что пора заблокировать документ и применить геометрию.
        /// </summary>
        Write
    };

    /// <summary>
    /// Главный управляющий шлюз между ObjectARX COM и C++/CLI.
    /// </summary>
    public ref class ArxBridgeCore sealed
    {
    internal:
        // Указатель на нативный вектор зарегистрированных COM-свойств
        static std::vector<IUnknown*>* m_registeredProperties;
        static std::vector<AcDbObjectId>* m_frameSelectedIds;
        // Переменная текущего состояния конечного автомата
        static BridgeState m_currentState;

    public:
        // Управление жизненным циклом нативного контейнера вектора
        static void Initialize();
        static void Terminate();

        // Потокобезопасные геттеры и сеттеры для переключения автомата
        static void SetState(BridgeState newState);
        static BridgeState GetState();

        // Методы регистрации динамических свойств на уровне AcDbEntity
        static void RegisterDynamicPropertyForClasses(
            System::String^ propertyName,
            System::String^ description,
            PropType type,
            System::Collections::Generic::List<System::String^>^ targetClasses
        );
        static void ArxBridgeCore::RegisterDynamicProperty(System::String^ propertyName, System::String^ propertyDescription, PropType propType);
        static void UnregisterAllProperties();
    };
}
