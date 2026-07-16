#pragma once

#ifndef OPM_CATEGORY_HELPER_H
#define OPM_CATEGORY_HELPER_H

// Объявляем нативный тип функции-колбека, используя базовые типы C++
// long заменяет PROPCAT, а void* заменяет BSTR* и managedHandle
typedef long(__stdcall* GetCategoryNameCallback)(void* managedHandle, long propcat, void* pbstrName);

// Опережающее объявление (forward declaration) для нативного интерфейса IUnknown
struct IUnknown;

#ifdef __cplusplus
extern "C" {
#endif

    // Функция-фабрика, которую мы вызываем из управляемого кода
    __declspec(dllimport) IUnknown* CreateCategoryProxy(
        void* managedHandle,
        GetCategoryNameCallback callback
    );

#ifdef __cplusplus
}
#endif

#endif // OPM_CATEGORY_HELPER_H
