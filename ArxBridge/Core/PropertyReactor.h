#pragma once

#include <dbmain.h>      
#include <acdocman.h>    

namespace ArxBridge {

    /// <summary>
    /// Нативный синглтон-реактор AutoCAD. 
    /// Отслеживает жизненный цикл документов (вкладок) и точечные изменения объектов в базах данных чертежей.
    /// </summary>
    class PropertyReactor : public AcApDocManagerReactor, public AcDbDatabaseReactor
    {
    private:
        PropertyReactor();
        ~PropertyReactor();

        // Запрещаем копирование и присваивание синглтона (Защита от дурака)
        PropertyReactor(const PropertyReactor&) = delete;
        PropertyReactor& operator=(const PropertyReactor&) = delete;

    public:
        /// <summary> Получить доступ к единственному экземпляру реактора. </summary>
        static PropertyReactor& getInstance();

        /// <summary> Включить мониторинг событий документов и баз данных AutoCAD. </summary>
        void startMonitoring();

        /// <summary> Безопасно отключить мониторинг и отписаться от всех событий. </summary>
        void stopMonitoring();

        // Перехват событий документов (Вкладок dwg-файлов)
        virtual void documentCreated(AcApDocument* pDoc) override;
        virtual void documentToBeDestroyed(AcApDocument* pDoc) override;

        // Перехват событий изменений внутри базы данных чертежа
        virtual void objectModified(const AcDbDatabase* pDb, const AcDbObject* pObj) override;
    };
}
