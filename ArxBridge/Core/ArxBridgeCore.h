#pragma once
#include <vector>
#include "Model/ModelSupport.h"

namespace ArxBridge {

    /// <summary>
    /// Состояния конечного автомата кадра палитры AutoCAD.
    /// Управляет тактами чтения и записи пакетов данных.
    /// </summary>
    public enum class BridgeState
    {
        Read,    // Режим ожидания/подготовки чтения контекста
        Connect, // Замок-блокировка повторных вызовов в рамках кадра
        Write    // Сигнал о том, что была произведена запись в кэш NewValue
    };

    /// <summary>
    /// Главный управляющий шлюз между ObjectARX COM и C++/CLI.
    /// </summary>
    public ref class ArxBridgeCore sealed
    {
    internal:
        // Указатель на нативный вектор зарегистрированных COM-свойств
        static std::vector<IUnknown*>* m_registeredProperties;

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
        static void ArxBridgeCore::RegisterDynamicProperty(System::String^ propertyName, System::String^ propertyDescription, PropType propType);
        static void UnregisterAllProperties();
    };
}
