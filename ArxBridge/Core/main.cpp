#include <map>
#include <functional>
namespace std {}
#include <windows.h>
#include "rxregsvc.h"
#include "arxHeaders.h"
#include "adslib.h" 
#include "axobjref.h"
#include "dynprops.h"

// Подключаем наши управляющие классы ядра строго по иерархии папок!
#include "ArxBridgeCore.h"
#include "PropertyEngine.h"
#include "PropertyReactor.h"
#include "Model/Property.h"

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
              
        
        
        return retCode;
    }

    // Метод срабатывает при принудительной выгрузке плагина пользователем
    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pkt) override {
        // Чисто тушим мониторинг чертежей dwg и полностью очищаем ОЗУ от долгоживущего кэша
        ArxBridge::PropertyReactor::getInstance().stopMonitoring();
        ArxBridge::PropertyEngine::Terminate();
        
        ArxBridge::ArxBridgeCore::UnregisterAllProperties();

        return AcRxDbxApp::On_kUnloadAppMsg(pkt);
    }
     

    virtual void RegisterServerComponents(void) override {
        // Оставляем пустой, если дополнительных COM-компонентов нет
    }
};

// Главная магия: макрос разворачивает функции acrxEntryPoint и acrxGetApiVersion для линкера
IMPLEMENT_ARX_ENTRYPOINT(CArxBridgeApp)
