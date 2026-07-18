#include "SelectedObjectData.h"
#include "Property.h"

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    // --- 1. Конструктор класса ---
    SelectedObjectData::SelectedObjectData(AcDbObjectId id)
    {
        // 1. Кэшируем числовой ID для нативного C++ движка
        m_numericId = id.asOldId();

        // 2. Кэшируем готовую .NET структуру для управляемых C# плагинов (каст происходит ОДИН раз)
        _managedId = Autodesk::AutoCAD::DatabaseServices::ObjectId(System::IntPtr(reinterpret_cast<void*>(m_numericId)));

        _properties = gcnew List<Property^>();
        _isModified = true;
    }

    // --- 2. Геттер для динамической проверки изменений пользователя ---
    bool SelectedObjectData::IsChanged::get()
    {
        if (_properties == nullptr) return false;

        for each (Property ^ x in _properties)
        {
            if (x->IsChanged) return true;
        }
        return false;
    }

    // --- 3. Быстрое извлечение нативного ID для ObjectARX функций ---
    AcDbObjectId SelectedObjectData::GetNativeId()
    {
        AcDbObjectId id;
        id.setFromOldId(m_numericId);
        return id;
    }

    // --- 4. Проверка идентичности объекта переданному нативному ID ---
    bool SelectedObjectData::IsIdenticalTo(AcDbObjectId id)
    {
        return m_numericId == id.asOldId();
    }
}
