#pragma once

// Включаем монолитные заголовки ядра ObjectARX через угловые скобки
#include <dbmain.h>      // Содержит AcDbDatabaseReactor и AcDbObject
#include <acdocman.h>    // Содержит AcApDocManagerReactor и AcApDocument

namespace ArxBridge {

    /// <summary>
    /// Полностью нативный С++ реактор сессии. Работает через статические .lib библиотеки SDK.
    /// Отслеживает жизненный цикл вкладок файлов и точечную модификацию объектов.
    /// </summary>
    class PropertyReactor : public AcApDocManagerReactor, public AcDbDatabaseReactor
    {
    private:
        PropertyReactor();
        ~PropertyReactor();

    public:
        static PropertyReactor& getInstance();

        void startMonitoring();
        void stopMonitoring();

        // Перехват событий документов (Вкладок dwg-файлов)
        virtual void documentCreated(AcApDocument* pDoc) override;
        virtual void documentToBeDestroyed(AcApDocument* pDoc) override;
        virtual void documentActivated(AcApDocument* pDoc) override;

        // Перехват событий изменений внутри базы данных чертежа
        virtual void objectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) override;
    };
}
