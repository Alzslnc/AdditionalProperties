#pragma once
#include "Model/Property.h"
#include "dbid.h" // Обязательный заголовок AutoCAD для AcDbObjectId

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    public ref class SelectedObjectData
    {
    private:
        // Нативный идентификатор. Занимает всего 8 байт, живет внутри управляемого объекта.
        AcDbStub* m_pNativeStub;

    public:
        // Конструктор для C++ модуля (принимает нативный ID)
        SelectedObjectData(AcDbObjectId id)
        {
            // Извлекаем чистый нативный указатель
            m_pNativeStub = reinterpret_cast<AcDbStub*>(id.asOldId());

            Properties = gcnew List<Property^>();
            ClassName = String::Empty;
            IsModified = true;
        }

        // --- ДЛЯ C# МОДУЛЯ (Управляемый интерфейс) ---

        // Возвращает стандартную структуру .NET без знака ^ (так как это ValueType)
        property Autodesk::AutoCAD::DatabaseServices::ObjectId ObjectId {
            Autodesk::AutoCAD::DatabaseServices::ObjectId get() {
                // Мгновенно собираем .NET структуру прямо из нативного указателя
                return Autodesk::AutoCAD::DatabaseServices::ObjectId(System::IntPtr(m_pNativeStub));
            }
        }

        property String^ ClassName;
        property List<Property^>^ Properties;
        property bool IsModified;

        property bool IsChanged {
            bool get() {
                if (!Properties) return false;
                for each (Property ^ x in Properties) {
                    if (x->IsChanged) return true;
                }
                return false;
            }
        }

        // --- ДЛЯ C++ МОДУЛЯ (Нативный интерфейс идентификации) ---

        // Метод быстрого получения нативного ID для функций ObjectARX
        AcDbObjectId GetNativeId() {
            AcDbObjectId id;
            id.setFromOldId(reinterpret_cast<Adesk::IntDbId>(m_pNativeStub));
            return id;
        }

        // Метод для проверки, принадлежит ли этот закэшированный объект нативному ID
        bool IsIdenticalTo(AcDbObjectId id) {
            return m_pNativeStub == reinterpret_cast<AcDbStub*>(id.asOldId());
        }
    };
}
