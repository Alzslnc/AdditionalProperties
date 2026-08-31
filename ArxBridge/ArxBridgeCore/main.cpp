#include <windows.h>
#include "arxHeaders.h"

#include "ArxBridgeCore.h"
#include "BridgeManager.h"
//#include "PropertyEngine.h"
#include "PropertyReactor.h"
#include "DocumentCacheManager.h"
#include "BridgePropertyWatcher.h"
#include "OpmMessageHook.h" 

// Главный нативный класс-наследник сессии приложения AutoCAD
class CArxBridgeApp : public AcRxDbxApp {
public:
    CArxBridgeApp() : AcRxDbxApp() {}

    // Метод срабатывает в момент загрузки .arx плагина в память AutoCAD
    virtual AcRx::AppRetCode On_kInitAppMsg(void* pkt) override {
        AcRx::AppRetCode retCode = AcRxDbxApp::On_kInitAppMsg(pkt);

        ArxBridge::ArxBridgeCore::Initialize();

        // 🚀 ОПТИМИЗАЦИЯ: Напрямую инициализируем менеджер кэша ОЗУ в обход транзитных методов
        ArxBridge::DocumentCacheManager::Initialize();

        // Запуск реакторов мониторинга документов и объектов чертежа
        ArxBridge::PropertyReactor::getInstance().startMonitoring();

        ArxBridge::BridgeManager::Initialize();

        OpmMessageHook::Start();

        return retCode;
    }

    // Метод срабатывает при принудительной выгрузке плагина пользователем
    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pkt) override {

        // Первым делом снимаем Windows-хук, чтобы поток AutoCAD больше не вызывал наш коллбэк
        OpmMessageHook::Stop();

        // Чисто тушим мониторинг чертежей dwg
        ArxBridge::PropertyReactor::getInstance().stopMonitoring();

        // 🚀 ОПТИМИЗАЦИЯ: Напрямую очищаем ОЗУ от долгоживущего кэша документов
        ArxBridge::DocumentCacheManager::Terminate();

        ArxBridge::BridgeManager::Terminate();

        ArxBridge::BridgePropertyWatcher::getInstance().UnregisterActiveContext(nullptr);

        // Отписываемся от палитры свойств AutoCAD (OPM)
        ArxBridge::ArxBridgeCore::UnregisterAllProperties();
        ArxBridge::ArxBridgeCore::Terminate();
      
        return AcRxDbxApp::On_kUnloadAppMsg(pkt);
    }

    virtual void RegisterServerComponents(void) override {
        // Оставляем пустой, если дополнительных COM-компонентов нет
    }
};

IMPLEMENT_ARX_ENTRYPOINT(CArxBridgeApp)
