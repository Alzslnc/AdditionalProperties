#include "OpmTriggerState.h"
#include <windows.h>
#include <mutex>

static std::mutex g_stateMutex;
static void (*g_notifyCallback)() = nullptr;
static PTP_TIMER g_nativeTimer = nullptr;

// Колбэк таймера: если за 50 мс его никто не потушил — это ЧЕСТНЫЙ текстбокс
static VOID CALLBACK NativeTimerCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer) {
    std::lock_guard<std::mutex> lock(g_stateMutex);

    if (g_notifyCallback != nullptr) {
        g_notifyCallback(); // Пинаем мост для подписки на управляемый Idle
    }
}

void OpmTriggerState::Initialize(void (*notifyCallback)()) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_notifyCallback = notifyCallback;

    if (g_nativeTimer == nullptr) {
        g_nativeTimer = CreateThreadpoolTimer(&NativeTimerCallback, nullptr, nullptr);
    }
}

// Метод LockModeChanged теперь работает исключительно как ТОРМОЗ (Глушитель)
bool OpmTriggerState::ProcessLockMode(const ACHAR* pGlobalCmdName) {
    std::lock_guard<std::mutex> lock(g_stateMutex);

    // 1. Базовая защита от nullptr
    if (pGlobalCmdName == nullptr) {
        if (g_nativeTimer != nullptr) {
            SetThreadpoolTimer(g_nativeTimer, nullptr, 0, 0);
        }
        return false;
    }

    // 2. Вычисляем длину строки
    size_t cmdLength = std::wcslen(pGlobalCmdName);

    // 3. ИСПРАВЛЕНО: Полное сито условий для гашения таймера.
    // Таймер текста БЕЗУСЛОВНО ТУШИТСЯ, если:
    // - Длина строки равна 0 (cmdLength == 0 или пустой замок L"")
    // - Первый символ равен L'\0'
    // - Первый символ является решеткой (pGlobalCmdName[0] == L'#')
    // - Прилетела команда отмены/завершения превью комбобокса (L"PREVIEW_END")
    if (cmdLength == 0 ||
        pGlobalCmdName[0] == L'\0' ||
        pGlobalCmdName[0] == L'#' ||
        std::wcscmp(pGlobalCmdName, L"PREVIEW_END") == 0)
    {
        if (g_nativeTimer != nullptr) {
            SetThreadpoolTimer(g_nativeTimer, nullptr, 0, 0); // Потушили!
        }
    }

    return false; // Нам больше не нужно возвращать true/false для комбобоксов
}

void OpmTriggerState::ProcessModelessEnded() {
    std::lock_guard<std::mutex> lock(g_stateMutex);

    // Взводим таймер ожидания "догоняющего" спам-замка на 50 мс
    if (g_nativeTimer != nullptr) {
        ULARGE_INTEGER dueTime;
        dueTime.QuadPart = (ULONGLONG)-(50 * 10000); // 50 мс

        FILETIME ft;
        ft.dwLowDateTime = dueTime.LowPart;
        ft.dwHighDateTime = dueTime.HighPart;

        SetThreadpoolTimer(g_nativeTimer, &ft, 0, 0);
    }
}

// Добавьте этот метод внутрь OpmTriggerState.cpp
void OpmTriggerState::Reset() {
    std::lock_guard<std::mutex> lock(g_stateMutex);

    // Принудительно выключаем нативный Win32-таймер Windows,
    // чтобы он не дотигал на закрытом чертеже
    if (g_nativeTimer != nullptr) {
        SetThreadpoolTimer(g_nativeTimer, nullptr, 0, 0);
    }
}