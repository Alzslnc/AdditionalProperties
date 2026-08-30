// OpmMessageHook.cpp
#include "OpmMessageHook.h"
#include <windows.h>
#include <cwchar>
#include <string>
#include <algorithm>

static HHOOK g_hOpmHook = NULL;
static void (*g_notifyCallback)() = nullptr;

// Функция для железобетонного подтверждения, что окно принадлежит Палитре Свойств AutoCAD
static bool IsTargetOpmWindow(HWND hWndChild) {
    HWND hWndParent = hWndChild;
    wchar_t className[256]; // Массив выделен корректно, переполнения стека не будет

    // Поднимаемся по иерархии окон вверх до контейнера палитры свойств
    for (int i = 0; i < 7 && hWndParent != NULL; i++) {
        if (::GetClassName(hWndParent, className, 256) > 0) {

            // Если дошли до базовой MFC-панели докера AutoCAD
            if (::wcsstr(className, L"AfxControlBar") != nullptr ||
                ::wcsstr(className, L"AfxWnd") != nullptr ||
                ::wcsstr(className, L"ATL:") != nullptr)
            {
                wchar_t windowText[256] = { 0 };
                if (::GetWindowTextW(hWndParent, windowText, 256) > 0) // Явно вызываем Wide-версию API
                {
                    // 1. Переводим весь буфер в нижний регистр силами Windows API
                    // (функция отлично понимает кириллицу и меняет символы прямо в массиве)
                    ::CharLowerW(windowText);

                    // 2. Создаем строку из уже подготовленного текста
                    std::wstring title(windowText);

                    // Ищем вхождение (теперь гарантированно в нижнем регистре!)
                    if (title.find(L"\u0441\u0432\u043e\u0439\u0441\u0442\u0432") != std::wstring::npos ||
                        title.find(L"propert") != std::wstring::npos)
                    {
                        return true; // ГАРАНТИРОВАНО И НАВСЕГДА!
                    }
                }
            }
        }

        // Шаг вверх к родителю
        hWndParent = ::GetAncestor(hWndParent, GA_PARENT);
    }
    return false;
}

// Процедура хука оконных сообщений Windows (вызывается синхронно потоком AutoCAD)
// Предположим, у вас есть два коллбэка (или один с параметрами), 
// чтобы управляемый код понимал, откуда прилетел пинок.
// Для примера добавим гипотетический g_notifyTextBoxCallback для текста.
// Если коллбэк один, можно передавать в него ID (например, 1 - комбо, 2 - текст).

static LRESULT CALLBACK OpmMessageHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
        return ::CallNextHookEx(g_hOpmHook, nCode, wParam, lParam);

    CWPSTRUCT* pCwp = (CWPSTRUCT*)lParam;

    if (pCwp->message == WM_COMMAND)
    {
        WORD wNotifyCode = HIWORD(pCwp->wParam);

        // 1. ЛОВИМ ФИНАЛЬНЫЙ ВЫБОР В КОМБОБОКСЕ
        if (wNotifyCode == CBN_SELCHANGE) // Код 1
        {
            HWND hWndCtrl = (HWND)pCwp->lParam;
            if (hWndCtrl != NULL && IsTargetOpmWindow(hWndCtrl))
            {
                if (g_notifyCallback != nullptr) {
                    g_notifyCallback(); // Сигнал: Сработал комбобокс
                }
            }
        }
        // 2. ЛОВИМ ВЫХОД ИЗ ТЕКСТОВОГО ПОЛЯ (Потеря фокуса)
        else if (wNotifyCode == EN_KILLFOCUS) // Код 512 (0x0200)
        {
            if (OpmMessageHook::IsChanged)
            {
                HWND hWndCtrl = (HWND)pCwp->lParam;
                if (hWndCtrl != NULL && IsTargetOpmWindow(hWndCtrl))
                {
                    // ПОЙМАНО: Текстбокс в палитре свойств потерял фокус.
                    // Вы можете вызвать специальный коллбэк для текстбокса, 
                    // либо взвести флаг в вашей системе триггеров.
                    if (g_notifyCallback != nullptr) {
                        g_notifyCallback(); // Сигнал: Сработал комбобокс
                    }
                }
            }
        }
    }

    return ::CallNextHookEx(g_hOpmHook, nCode, wParam, lParam);
}


void OpmMessageHook::Start(void (*notifyCallback)()) {
    if (g_hOpmHook == NULL && notifyCallback != nullptr) {
        g_notifyCallback = notifyCallback;

        // Устанавливаем локальный хук оконных процедур (WH_CALLWNDPROC).
        // Передаем GetCurrentThreadId(), чтобы хукать ТОЛЬКО поток самого AutoCAD. 
        // Это абсолютно безопасно, не требует административных прав и глобальных DLL-инъекций.
        g_hOpmHook = ::SetWindowsHookEx(
            WH_CALLWNDPROC,
            OpmMessageHookProc,
            NULL,
            ::GetCurrentThreadId()
        );
    }
}

void OpmMessageHook::Stop() {
    if (g_hOpmHook != NULL) {
        ::UnhookWindowsHookEx(g_hOpmHook);
        g_hOpmHook = NULL;
        g_notifyCallback = nullptr;
    }
}
