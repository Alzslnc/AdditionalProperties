// OpmTriggerState.h
#pragma once

#include <acutads.h> // Для поддержки типа ACHAR в AutoCAD

class OpmTriggerState {
public:
    // Функция инициализации. Принимает нативный указатель на функцию-колбэк
    static void Initialize(void (*notifyCallback)());

    // Принудительный сброс и отключение таймера при смене/закрытии документа
    static void Reset();

    // Вызывается из нативного OpmPaletteSubscription::OnDocumentLockModeChanged
    // Возвращает true, если это PREVIEW_END (пора обновлять комбобокс)
    static bool ProcessLockMode(const ACHAR* pGlobalCmdName);

    // Вызывается из управляемого OnModelessOperationEnded
    static void ProcessModelessEnded();
};
