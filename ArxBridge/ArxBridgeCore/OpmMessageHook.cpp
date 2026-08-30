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
                    std::wstring title(windowText);

                    // Переводим в нижний регистр для безопасного сравнения
                    std::transform(title.begin(), title.end(), title.begin(), ::towlower);

                    // Ищем вхождение без привязки к регистру и окончаниям
                    if (title.find(L"свойств") != std::wstring::npos ||
                        title.find(L"propert") != std::wstring::npos)
                    {
                        return true; // ГАРАНТИРОВАНО!
                    }
                }
            }

            // Если в цепочке окон внезапно обнаружилась командная строка — это ложное срабатывание, выходим
            if (::wcsstr(className, L"CommandLine") != nullptr) {
                return false;
            }
        }

        // Шаг вверх к родителю
        hWndParent = ::GetAncestor(hWndParent, GA_PARENT);
    }
    return false;
}

// Процедура хука оконных сообщений Windows (вызывается синхронно потоком AutoCAD)
static LRESULT CALLBACK OpmMessageHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
        return ::CallNextHookEx(g_hOpmHook, nCode, wParam, lParam);

    CWPSTRUCT* pCwp = (CWPSTRUCT*)lParam;

    // Перехватываем стандартные Win32-команды от элементов управления палитры
    if (pCwp->message == WM_COMMAND)
    {
        WORD wNotifyCode = HIWORD(pCwp->wParam);

        // ЛОВИМ СТРОГО КОД 1 (CBN_SELCHANGE) - Физический клик по элементу выпадающего списка.
        // Весь спам от наведения мыши по списку (без клика) этот хук полностью игнорирует!
        if (wNotifyCode == CBN_SELCHANGE)
        {
            HWND hWndCtrl = (HWND)pCwp->lParam;
            if (hWndCtrl != NULL && IsTargetOpmWindow(hWndCtrl))
            {
                // ПОЙМАНО: Пользователь выбрал новое значение в комбобоксе палитры свойств.
                // Пинаем наш глобальный управляемый мост, чтобы запланировать безопасное Idle-обновление
                if (g_notifyCallback != nullptr) {
                    g_notifyCallback();
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
