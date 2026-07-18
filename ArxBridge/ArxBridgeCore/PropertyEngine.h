#pragma once
#include <vector>
#include "dbid.h"

namespace ArxBridge {

    /// <summary>
    /// Высокоуровневый оркестратор OPM-моста. 
    /// Координирует пакетный сбор объектов с экрана чертежа и фиксацию изменений обратно в базу данных.
    /// </summary>
    class PropertyEngine
    {
    private:
        PropertyEngine() = delete;

    public:     

        // 🚀 ГЛАВНЫЕ ОРКЕСТРАТОРЫ СЧИТЫВАНИЯ И ЗАПИСИ КОНТЕКСТА ЧЕРТЕЖА

        /// <summary> Пакетный сбор данных: считывает свойства для всего набора переданных ID за один проход. </summary>
        static void PrepareSelectionContext(AcDbDatabase* pDb, const std::vector<AcDbObjectId>& selectedIds);

        /// <summary> Нативный оркестратор сбора экрана через системный предварительный выбор (SelectImplied). </summary>
        static void PrepareInitialSelectionContext(const std::vector<AcDbObjectId>& filterTypeIds);

        /// <summary> Фиксация изменений: открывает транзакцию записи и применяет измененные в OPM свойства. </summary>
        static void CommitSelectionChanges(AcDbDatabase* pDb);
    };
}
