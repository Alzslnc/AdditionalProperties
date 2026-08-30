// OpmMessageHook.h
#pragma once

#include <acutads.h> // Для поддержки типов AutoCAD (ACHAR), если нужно

class OpmMessageHook {
public:
    // Инициализация хука. Принимает адрес нативной функции-колбэка (__cdecl)
    static void Start(void (*notifyCallback)());

    // Принудительное снятие хука при выгрузке плагина из AutoCAD
    static void Stop();
};
