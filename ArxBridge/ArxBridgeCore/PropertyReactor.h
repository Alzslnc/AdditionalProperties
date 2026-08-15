#pragma once

#include <dbmain.h>      
#include <acdocman.h>    
#include <vector>

namespace ArxBridge {

    /// <summary>
    /// Базовый интерфейс для всех кастомных нативных подписок.
    /// Позволяет ядру PropertyReactor транслировать события AutoCAD в изолированные модули.
    /// </summary>
    class BridgeSubscriptionBase {
    public:
        virtual ~BridgeSubscriptionBase() = default;

        virtual void OnDocumentCreated(AcApDocument* pDoc) {}
        virtual void OnDocumentToBeDestroyed(AcApDocument* pDoc) {}
        virtual void OnObjectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) {}
    };

    /// <summary>
    /// Нативный синглтон-менеджер подписок AutoCAD. 
    /// Отслеживает глобальный жизненный цикл документов и баз данных чертежей,
    /// транслируя события во все зарегистрированные нативные кастомные сервисы.
    /// </summary>
    class PropertyReactor : public AcApDocManagerReactor, public AcDbDatabaseReactor
    {
    private:
        PropertyReactor();
        ~PropertyReactor();

        PropertyReactor(const PropertyReactor&) = delete;
        PropertyReactor& operator=(const PropertyReactor&) = delete;

        // Внутренний вектор для хранения зарегистрированных нативных подписок
        std::vector<BridgeSubscriptionBase*> m_subscriptions;

    public:
        /// <summary> Получить доступ к единственному экземпляру ядра подписок. </summary>
        static PropertyReactor& getInstance();

        /// <summary> Включить нативный мониторинг событий AutoCAD и инициализировать пакет подписок. </summary>
        void startMonitoring();

        /// <summary> Безопасно отключить нативный мониторинг и очистить пакет подписок. </summary>
        void stopMonitoring();

        // Перехват нативных событий документов (Вкладок dwg-файлов)
        virtual void documentCreated(AcApDocument* pDoc) override;
        virtual void documentToBeDestroyed(AcApDocument* pDoc) override;

        // Перехват нативных событий изменений внутри базы данных чертежа
        virtual void objectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) override;
    };
} // namespace ArxBridge
