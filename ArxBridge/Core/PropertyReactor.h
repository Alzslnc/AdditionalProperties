#pragma once

#include <dbmain.h>      
#include <acdocman.h>    

namespace ArxBridge {

    class PropertyReactor : public AcApDocManagerReactor, public AcDbDatabaseReactor
    {
    private:
        PropertyReactor();
        ~PropertyReactor();

        // Запрещаем копирование и присваивание синглтона (Защита от дурака)
        PropertyReactor(const PropertyReactor&) = delete;
        PropertyReactor& operator=(const PropertyReactor&) = delete;

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
