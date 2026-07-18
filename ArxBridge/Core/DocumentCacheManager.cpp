#include "DocumentCacheManager.h"
#include "../Model/SelectedObjectData.h"
#include "../Model/Property.h"

using namespace System;
using namespace System::Collections::Generic;

namespace ArxBridge {

    // 1. Выделение памяти под низкоуровневые контейнеры кэша чертежей
    std::map<AcDbDatabase*, gcroot<List<SelectedObjectData^>^>> DocumentCacheManager::m_multiDocCache;
    gcroot<List<SelectedObjectData^>^> DocumentCacheManager::m_currentSelectionCache;

    void DocumentCacheManager::Initialize() {
        if (static_cast<List<SelectedObjectData^>^>(m_currentSelectionCache) == nullptr) {
            m_currentSelectionCache = gcnew List<SelectedObjectData^>();
        }
    }

    void DocumentCacheManager::Terminate() {
        m_multiDocCache.clear();
        if (static_cast<List<SelectedObjectData^>^>(m_currentSelectionCache) != nullptr) {
            m_currentSelectionCache->Clear();
        }
    }

    void DocumentCacheManager::ClearCache(AcDbDatabase* pDb) {
        if (pDb != nullptr && m_multiDocCache.find(pDb) != m_multiDocCache.end()) {
            m_multiDocCache[pDb]->Clear();
            m_multiDocCache.erase(pDb);
        }
    }

    void DocumentCacheManager::InvalidateObject(AcDbDatabase* pDb, AcDbObjectId id) {
        if (pDb == nullptr || !id.isValid() || m_multiDocCache.find(pDb) == m_multiDocCache.end()) return;

        List<SelectedObjectData^>^ dbCache = m_multiDocCache[pDb];
        for (int i = 0; i < dbCache->Count; ++i) {
            if (dbCache[i]->IsIdenticalTo(id)) {
                // Сердце ленивой актуализации: взводим флаг модификации
                dbCache[i]->IsModified = true;
                break;
            }
        }
    }

    SelectedObjectData^ DocumentCacheManager::FindObjectInCache(AcDbDatabase* pDb, AcDbObjectId id) {
        if (static_cast<List<SelectedObjectData^>^>(m_currentSelectionCache) == nullptr) return nullptr;

        List<SelectedObjectData^>^ currentSelection = m_currentSelectionCache;
        for (int i = 0; i < currentSelection->Count; ++i) {
            if (currentSelection[i] != nullptr && currentSelection[i]->IsIdenticalTo(id)) {
                return currentSelection[i];
            }
        }
        return nullptr;
    }

    Property^ DocumentCacheManager::FindPropertyInCache(AcDbDatabase* pDb, AcDbObjectId id, const std::wstring& propName) {
        SelectedObjectData^ objData = FindObjectInCache(pDb, id);
        if (objData == nullptr || objData->Properties == nullptr) return nullptr;

        // ОПТИМИЗАЦИЯ: Переводим имя свойства в .NET строку ОДИН раз перед циклом вместо PtrToStringChars в цикле
        String^ managedPropName = gcnew String(propName.c_str());

        for (int i = 0; i < objData->Properties->Count; ++i) {
            Property^ prop = objData->Properties[i];
            if (prop != nullptr && String::Equals(prop->Name, managedPropName, StringComparison::OrdinalIgnoreCase)) {
                return prop;
            }
        }
        return nullptr;
    }

    void DocumentCacheManager::SetSelectionCache(AcDbDatabase* pDb, List<SelectedObjectData^>^ newSelection) {
        m_currentSelectionCache = newSelection;
    }

    List<SelectedObjectData^>^ DocumentCacheManager::GetCurrentSelectionCache() {
        return m_currentSelectionCache;
    }
}
