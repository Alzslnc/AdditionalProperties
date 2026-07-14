
#include <windows.h>
#include "rxregsvc.h"
#include "arxHeaders.h"
#include "adslib.h" 

// Подключаем наши управляющие классы ядра строго по иерархии папок!
#include "PropertyEngine.h"
#include "PropertyReactor.h"
#include "ArxBridgeCore.h"

using namespace Autodesk::AutoCAD::Runtime;
using namespace Autodesk::AutoCAD::DatabaseServices;
// Для доступа к фабрикам OPM в современных версиях:
using namespace Autodesk::AutoCAD::Internal::PropertyInspector;

// Главный нативный класс-наследник сессии приложения AutoCAD
class CArxBridgeApp : public AcRxDbxApp {
public:
    CArxBridgeApp() : AcRxDbxApp() {}

    // Метод срабатывает в момент загрузки .arx плагина в память AutoCAD
    virtual AcRx::AppRetCode On_kInitAppMsg(void* pkt) override {
        AcRx::AppRetCode retCode = AcRxDbxApp::On_kInitAppMsg(pkt);
        
        // На старте инициализируем С++ кэш и запускаем слежку реактора баз данных
        ArxBridge::PropertyEngine::Initialize();
        ArxBridge::PropertyReactor::getInstance().startMonitoring();
        
        //// 3. РЕГИСТРАЦИЯ В ПАЛИТРЕ: Рассказываем AutoCAD о существовании нашего COM-класса!
        //AcOpmPropertyManager* pOpmMgr = acOpmGetPropertyManager();
        //if (pOpmMgr != nullptr) {
        //    // Привязываем наш мост ко ВСЕМ графическим примитивам AutoCAD (AcDbEntity)
        //    // Без этой строки AutoCAD никогда не вызовет методы Connect и GetCurrentValueData!
        //    pOpmMgr->RegisterPropertyExtension(AcDbEntity::desc(), CLSID_DynamicPropertyBridge);
        //}

        // 1. Создаем экземпляр вашего управляемого COM-транслятора
        //DynamicPropertyBridge^ myProperty = gcnew DynamicPropertyBridge(name, type, isReadOnly);

        //// 2. Получаем доступ к расширению инспектора свойств через глобальный менеджер для нужного типа объектов (например, Entity)
        //System::Type^ entityType = Autodesk::AutoCAD::DatabaseServices::Entity::typeid;

        //// В современном API за регистрацию отвечает фабрика инспектора свойств:
        //Autodesk::AutoCAD::Internal::PropertyInspector::IPropertyManager^ propMgr =
        //    Autodesk::AutoCAD::Internal::PropertyInspector::PropertyInspectorFactory::GetPropertyManager(entityType);

        //// 3. Добавляем динамическое свойство
        //if (propMgr != nullptr) {
        //    propMgr->AddProperty(myProperty);
        //}

        return retCode;
    }

    // Метод срабатывает при принудительной выгрузке плагина пользователем
    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pkt) override {
        // Чисто тушим мониторинг чертежей dwg и полностью очищаем ОЗУ от долгоживущего кэша
        ArxBridge::PropertyReactor::getInstance().stopMonitoring();
        ArxBridge::PropertyEngine::Terminate();
        
        return AcRxDbxApp::On_kUnloadAppMsg(pkt);
    }
     

    virtual void RegisterServerComponents(void) override {
        // Оставляем пустой, если дополнительных COM-компонентов нет
    }
};

// Главная магия: макрос разворачивает функции acrxEntryPoint и acrxGetApiVersion для линкера
IMPLEMENT_ARX_ENTRYPOINT(CArxBridgeApp)
