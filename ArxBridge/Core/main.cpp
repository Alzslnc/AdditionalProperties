#include <windows.h>
#include "arxHeaders.h"

#include "ArxBridgeCore.h"
#include "BridgeManager.h"
#include "PropertyEngine.h"
#include "PropertyReactor.h"
#include "Model/ModelSupport.h"

// Используем наше пространство имен, чтобы убрать визуальный шум из методов
using namespace ArxBridge;

// Главный нативный класс-наследник сессии приложения AutoCAD
class CArxBridgeApp : public AcRxDbxApp {
public:
    CArxBridgeApp() : AcRxDbxApp() {}

    // Метод срабатывает в момент загрузки .arx плагина в память AutoCAD
    virtual AcRx::AppRetCode On_kInitAppMsg(void* pkt) override {
        AcRx::AppRetCode retCode = AcRxDbxApp::On_kInitAppMsg(pkt);
        ArxBridgeCore::Initialize();
        // Код стал чистым: убрали дублирующиеся префиксы ArxBridge::
        PropertyEngine::Initialize();
        PropertyReactor::getInstance().startMonitoring();
        BridgeManager::Initialize();
        return retCode;
    }

    // Метод срабатывает при принудительной выгрузке плагина пользователем
    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pkt) override {
        // Чисто тушим мониторинг чертежей dwg и полностью очищаем ОЗУ от долгоживущего кэша
        PropertyReactor::getInstance().stopMonitoring();
        PropertyEngine::Terminate();
        BridgeManager::Terminate();

        // Отписываемся от панели свойств
        ArxBridgeCore::UnregisterAllProperties();
        ArxBridgeCore::Terminate();
        return AcRxDbxApp::On_kUnloadAppMsg(pkt);
    }

    virtual void RegisterServerComponents(void) override {
        // Оставляем пустой, если дополнительных COM-компонентов нет
    }
};

IMPLEMENT_ARX_ENTRYPOINT(CArxBridgeApp)
