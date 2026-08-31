#include "OpmMessageHook.h"
#include "BridgePropertyWatcher.h"
#include <windows.h>
#include <cwchar>
#include <string>
#include <algorithm>
#include <vector>
#include <unordered_set> // 🚀 Для мгновенной проверки HWND за O(1)

// Инициализация статического флага
bool OpmMessageHook::IsChanged = false;

// Глобальные переменные Win32-хука
static HHOOK g_hOpmHook = NULL;

// 🚀 КЭШ ДЕСКРИПТОРОВ ОКОН: Полностью исключает постоянную "молотилку" строк через WinAPI
static std::unordered_set<HWND> g_targetWindowsCache;
static std::unordered_set<HWND> g_ignoredWindowsCache;

// Предварительное объявление нашего встроенного коллбэка
void __cdecl GlobalComboHookCallback();

// Оптимизированная функция проверки принадлежности окна Палитре Свойств AutoCAD
static bool IsTargetOpmWindow(HWND hWndChild) {
    if (hWndChild == NULL) return false;

    // 1. Быстрая проверка через хэш-таблицы (без вызовов функций ОС)
    if (g_targetWindowsCache.count(hWndChild) > 0) return true;
    if (g_ignoredWindowsCache.count(hWndChild) > 0) return false;

    HWND hWndParent = hWndChild;
    wchar_t className[256];

    // 2. Медленный подъем по иерархии (выполняется только ОДИН раз для каждого нового HWND)
    for (int i = 0; i < 7 && hWndParent != NULL; i++) {
        if (::GetClassNameW(hWndParent, className, 256) > 0) {

            // Проверяем базовые MFC/ATL классы докеров AutoCAD
            if (::wcsstr(className, L"AfxControlBar") != nullptr ||
                ::wcsstr(className, L"AfxWnd") != nullptr ||
                ::wcsstr(className, L"ATL:") != nullptr)
            {
                wchar_t windowText[256] = { 0 };
                if (::GetWindowTextW(hWndParent, windowText, 256) > 0)
                {
                    ::CharLowerW(windowText);
                    std::wstring title(windowText);

                    // Ищем вхождение ключевых слов (рус/eng)
                    if (title.find(L"\u0441\u0432\u043e\u0439\u0441\u0442\u0432") != std::wstring::npos ||
                        title.find(L"propert") != std::wstring::npos)
                    {
                        g_targetWindowsCache.insert(hWndChild); // Запоминаем как валидное
                        return true;
                    }
                }
            }
        }
        hWndParent = ::GetAncestor(hWndParent, GA_PARENT);
    }

    g_ignoredWindowsCache.insert(hWndChild); // Запоминаем как чужое, чтобы больше не тратить процессор
    return false;
}

// Фильтрующая процедура хука оконных сообщений (вызывается синхронно в потоке AutoCAD)
static LRESULT CALLBACK OpmMessageHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Стандартный выход, если сообщение не для нас
    if (nCode < 0)
        return ::CallNextHookEx(g_hOpmHook, nCode, wParam, lParam);

    // 🚀 ПЕРВАЯ ЛИНИЯ ОБОРОНЫ: Если в AutoCAD вообще ничего не менялось,
    // мы мгновенно выходим, экономя такты процессора на каждом чихе UI.
    if (!OpmMessageHook::IsChanged)
        return ::CallNextHookEx(g_hOpmHook, nCode, wParam, lParam);

    CWPSTRUCT* pCwp = (CWPSTRUCT*)lParam;

    if (pCwp->message == WM_COMMAND)
    {
        WORD wNotifyCode = HIWORD(pCwp->wParam);

        // 1. ЛОВИМ И КОМБОБОКСЫ, И ТЕКСТБОКСЫ
        if (wNotifyCode == CBN_SELCHANGE || wNotifyCode == EN_KILLFOCUS)
        {
            HWND hWndCtrl = (HWND)pCwp->lParam;

            // Проверяем окно (теперь эта проверка дергается только при реальном изменении!)
            if (IsTargetOpmWindow(hWndCtrl))
            {
                GlobalComboHookCallback(); // Мгновенный прямой вызов updates

                // Сбрасываем флаг, так как изменение успешно обработано палитрой
                OpmMessageHook::IsChanged = false;
            }
        }
    }

    return ::CallNextHookEx(g_hOpmHook, nCode, wParam, lParam);
}


// Инициализация хука
void OpmMessageHook::Start() {
    if (g_hOpmHook == NULL) {
        // Гарантированно очищаем кэш дескрипторов при старте сессии
        g_targetWindowsCache.clear();
        g_ignoredWindowsCache.clear();

        // Ставим локальный хук строго на поток самого AutoCAD (GetCurrentThreadId)
        g_hOpmHook = ::SetWindowsHookEx(
            WH_CALLWNDPROC,
            OpmMessageHookProc,
            NULL,
            ::GetCurrentThreadId()
        );
    }
}

// Снятие хука
void OpmMessageHook::Stop() {
    if (g_hOpmHook != NULL) {
        ::UnhookWindowsHookEx(g_hOpmHook);
        g_hOpmHook = NULL;

        // Освобождаем память кэша окон
        g_targetWindowsCache.clear();
        g_ignoredWindowsCache.clear();
    }
}

// 🚀 РЕАЛИЗАЦИЯ НАШЕГО ВСТРОЕННОГО ПРЯМОГО КОЛЛБЭКА
void __cdecl GlobalComboHookCallback() {   
    // Синхронно пинаем COM-интерфейс палитры свойств для моментальной перерисовки
    ArxBridge::BridgePropertyWatcher::getInstance().Refresh();
}
