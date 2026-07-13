#pragma once

namespace ArxBridge {

    /// <summary>
    /// Управляемая точка входа для C++ моста.
    /// Отвечает за общую инициализацию и выгрузку инфраструктуры палитры свойств.
    /// </summary>
    public ref class ArxBridgeCore sealed
    {
    public:
        // Вызывается один раз при загрузке .arx / .dll модуля в память AutoCAD
        static void Initialize();

        // Вызывается при выгрузке плагина или закрытии сессии AutoCAD
        static void Terminate();
    };
}
