//#pragma once
//
//
//#include <vector>
//#include "dynprops.h"
//#include <comdef.h>

// 1. СНАЧАЛА ПРИНУДИТЕЛЬНО ПОДКЛЮЧАЕМ СТАНДАРТНЫЕ КОНТЕЙНЕРЫ C++
// Это даст файлу dbid.h понимание, что такое std::hash в .NET 8 / 10
#include <map>
#include <functional> 
#include <vector>

// 2. Временно изолируем имена, чтобы System::IServiceProvider не конфликтовал
#pragma push_macro("ServiceProvider")
#undef ServiceProvider

// 3. Теперь подключаем базовые типы AutoCAD
#include "rxobject.h" 
#include "dbmain.h"    // Этот файл внутри себя вызывает dbid.h
#include "dynprops.h"

#pragma pop_macro("ServiceProvider")

namespace ArxBridge {
       
    // Обязательно public ref class, чтобы его увидели C# модули и BridgeManager!
    public ref class ArxBridgeCore sealed
    {
    internal:
        // Нативный контейнер (std::vector) храним под флагом internal. 
        // C# его не увидит, но внутри этого же C++/CLI проекта (в BridgeManager) им можно пользоваться.
        static std::vector<IUnknown*>* m_registeredProperties = new std::vector<IUnknown*>();

    public:
        // Метод регистрации, который теперь принимает управляемую строку
        static void RegisterDynamicProperty(System::String^ propertyName);

        // Массовая выгрузка
        static void UnregisterAllProperties();
    };
}
