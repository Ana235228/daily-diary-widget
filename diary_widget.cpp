#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#pragma comment(lib, "gdiplus.lib")

#define MAX_ZAPISEY 1000
#define MAX_DLINA 512
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 1001
#define ID_HOTKEY 1
#define ID_TIMER 1

#define ID_EDIT_TEXT      2001
#define ID_EDIT_PRIORITY  2002
#define ID_BTN_ADD        2003
#define ID_LIST           2004
#define ID_BTN_DELETE     2005
#define ID_BTN_CLOSE      2006

#define IDM_OPEN       4001
#define IDM_WIDGET     4002
#define IDM_AUTOSTART  4003
#define IDM_EXIT       4004

#define APP_NAME L"DailyDiaryWidget"
#define APP_TITLE L"Ежедневник"

typedef struct {
    int id;
    wchar_t date[11];
    wchar_t time[6];
    wchar_t text[MAX_DLINA];
    int priority;
} Zapis;

Zapis diary[MAX_ZAPISEY];
int count = 0;

HINSTANCE hInst;
HWND hMainWnd = NULL;
HWND hList, hEditText, hEditPrio;
HWND hPopupWnd = NULL;
NOTIFYICONDATAW nid;
int showingPopup = 0;
HICON hIconColor = NULL;
HICON hIconGray  = NULL;
ULONG_PTR gdiplusToken = 0;

const wchar_t *FILE_NAME = L"diary.dat";
const wchar_t *REG_RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

HICON createDiaryIcon(int size, BOOL gray) {
    using namespace Gdiplus;

    Bitmap bitmap(size, size, PixelFormat32bppARGB);
    Graphics g(&bitmap);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    g.Clear(Color(0, 0, 0, 0));

    float s = (float)size;
    float margin = s * 0.12f;
    float radius = s * 0.18f;

    Color coverColor = gray ? Color(255, 120, 120, 130)
                            : Color(255, 60, 110, 210);
    Color coverDark  = gray ? Color(255, 80, 80, 90)
                            : Color(255, 35, 75, 160);
    Color pagesColor = gray ? Color(255, 220, 220, 225)
                            : Color(255, 250, 250, 245);
    Color ribbonColor = gray ? Color(255, 160, 160, 170)
                             : Color(255, 230, 70, 90);

    GraphicsPath coverPath;
    float x = margin, y = margin;
    float w = s - 2 * margin, h = s - 2 * margin;

    coverPath.AddArc(x, y, radius, radius, 180, 90);
    coverPath.AddArc(x + w - radius, y, radius, radius, 270, 90);
    coverPath.AddArc(x + w - radius, y + h - radius, radius, radius, 0, 90);
    coverPath.AddArc(x, y + h - radius, radius, radius, 90, 90);
    coverPath.CloseFigure();

    LinearGradientBrush coverBrush(PointF(x, y), PointF(x, y + h),
                                   coverColor, coverDark);
    g.FillPath(&coverBrush, &coverPath);

    float pageLeft = x + w * 0.12f;
    float pageRight = x + w - w * 0.08f;
    float pageTop = y + h * 0.10f;
    float pageBottom = y + h * 0.90f;

    GraphicsPath pagesPath;
    float pr = radius * 0.5f;
    pagesPath.AddArc(pageLeft, pageTop, pr, pr, 180, 90);
    pagesPath.AddArc(pageRight - pr, pageTop, pr, pr, 270, 90);
    pagesPath.AddArc(pageRight - pr, pageBottom - pr, pr, pr, 0, 90);
    pagesPath.AddArc(pageLeft, pageBottom - pr, pr, pr, 90, 90);
    pagesPath.CloseFigure();

    SolidBrush pagesBrush(pagesColor);
    g.FillPath(&pagesBrush, &pagesPath);

    Pen linePen(gray ? Color(180, 150, 150, 150)
                     : Color(180, 120, 120, 130), s * 0.02f);
    for (int i = 0; i < 3; i++) {
        float ly = pageTop + h * (0.22f + i * 0.18f);
        g.DrawLine(&linePen,
                   pageLeft + w * 0.05f, ly,
                   pageRight - w * 0.05f, ly);
    }

    GraphicsPath ribbon;
    float rx = x + w * 0.55f;
    float ry = y - s * 0.02f;
    float rw = w * 0.12f;
    float rh = h * 0.38f;
    ribbon.AddLine(rx, ry, rx + rw, ry);
    ribbon.AddLine(rx + rw, ry, rx + rw, ry + rh);
    ribbon.AddLine(rx + rw, ry + rh, rx + rw / 2, ry + rh - rh * 0.25f);
    ribbon.AddLine(rx + rw / 2, ry + rh - rh * 0.25f, rx, ry + rh);
    ribbon.CloseFigure();

    SolidBrush ribbonBrush(ribbonColor);
    g.FillPath(&ribbonBrush, &ribbon);

    HICON hIcon = NULL;
    bitmap.GetHICON(&hIcon);

    return hIcon;
}

BOOL isAutostartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0,
                      KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    wchar_t value[MAX_PATH] = {0};
    DWORD size = sizeof(value);
    DWORD type = 0;
    BOOL result = FALSE;

    if (RegQueryValueExW(hKey, APP_NAME, NULL, &type,
                         (LPBYTE)value, &size) == ERROR_SUCCESS
        && type == REG_SZ)
        result = TRUE;

    RegCloseKey(hKey);
    return result;
}

void setAutostart(BOOL enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0,
                      KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        MessageBoxW(NULL, L"Не удалось открыть реестр для автозапуска.",
                    L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);

        wchar_t quoted[MAX_PATH + 4];
        swprintf(quoted, MAX_PATH + 4, L"\"%s\"", path);

        RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
                       (const BYTE*)quoted,
                       (DWORD)((wcslen(quoted) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, APP_NAME);
    }

    RegCloseKey(hKey);
}

void getCurrentDate(wchar_t *buffer) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    swprintf(buffer, 11, L"%02d.%02d.%04d",
             tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900);
}

void getCurrentTime(wchar_t *buffer) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    swprintf(buffer, 6, L"%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
}

void saveToFile() {
    FILE *f = _wfopen(FILE_NAME, L"wb");
    if (!f) return;
    fwrite(&count, sizeof(int), 1, f);
    fwrite(diary, sizeof(Zapis), count, f);
    fclose(f);
}

void loadFromFile() {
    FILE *f = _wfopen(FILE_NAME, L"rb");
    if (!f) return;
    fread(&count, sizeof(int), 1, f);
    if (count > MAX_ZAPISEY) count = MAX_ZAPISEY;
    fread(diary, sizeof(Zapis), count, f);
    fclose(f);
}

void refreshList() {
    SendMessage(hList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < count; i++) {
        wchar_t line[600];
        const wchar_t *prio[] = {L"", L"[Низк]", L"[Сред]", L"[Выс]"};
        swprintf(line, 600, L"%d  %s %s %s  %s",
                 diary[i].id, diary[i].date, diary[i].time,
                 prio[diary[i].priority], diary[i].text);
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)line);
    }
}

void addZapis(HWND hWnd) {
    if (count >= MAX_ZAPISEY) {
        MessageBox(hWnd, L"Ежедневник заполнен!", L"Ошибка", MB_OK | MB_ICONWARNING);
        return;
    }

    wchar_t text[MAX_DLINA];
    GetWindowTextW(hEditText, text, MAX_DLINA);
    if (wcslen(text) == 0) {
        MessageBox(hWnd, L"Введите текст записи!", L"Внимание", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t prioStr[16];
    GetWindowTextW(hEditPrio, prioStr, 16);
    int prio = _wtoi(prioStr);
    if (prio < 1 || prio > 3) prio = 2;

    Zapis *z = &diary[count];
    z->id = (count == 0) ? 1 : diary[count - 1].id + 1;
    getCurrentDate(z->date);
    getCurrentTime(z->time);
    wcscpy(z->text, text);
    z->priority = prio;

    count++;
    saveToFile();
    refreshList();
    SetWindowTextW(hEditText, L"");
}

void deleteZapis(HWND hWnd) {
    int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        MessageBox(hWnd, L"Выберите запись для удаления.",
                   L"Внимание", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (MessageBox(hWnd, L"Удалить выбранную запись?", L"Подтверждение",
                   MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    for (int i = sel; i < count - 1; i++)
        diary[i] = diary[i + 1];
    count--;
    saveToFile();
    refreshList();
}

HRGN createRoundedRegion(int w, int h, int radius) {
    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1,
                                  radius * 2, radius * 2);
    return rgn;
}

LRESULT CALLBACK PopupProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            HBRUSH bg = CreateSolidBrush(RGB(45, 45, 55));
            FillRect(memDC, &rc, bg);
            DeleteObject(bg);

            SetBkMode(memDC, TRANSPARENT);

            HFONT hFontTitle = CreateFontW(20, 0, 0, 0, FW_BOLD, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
            HFONT hFontText = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
            HFONT hFontHint = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");

            SetTextColor(memDC, RGB(255, 255, 255));
            SelectObject(memDC, hFontTitle);
            RECT rcTitle = {15, 10, rc.right, 40};
            DrawTextW(memDC, L"Ежедневник", -1, &rcTitle,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(memDC, hFontText);

            int y = 45;
            int shown = 0;
            int start = count > 7 ? count - 7 : 0;

            for (int i = start; i < count && shown < 7; i++, shown++) {
                RECT rcItem = {15, y, rc.right - 15, y + 22};

                COLORREF c = RGB(200, 200, 200);
                if (diary[i].priority == 1) c = RGB(120, 220, 120);
                else if (diary[i].priority == 2) c = RGB(250, 220, 100);
                else if (diary[i].priority == 3) c = RGB(255, 120, 120);
                SetTextColor(memDC, c);

                wchar_t line[600];
                swprintf(line, 600, L"%s %s  %s",
                         diary[i].date, diary[i].time, diary[i].text);
                DrawTextW(memDC, line, -1, &rcItem,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                y += 24;
            }

            if (count == 0) {
                SetTextColor(memDC, RGB(180, 180, 180));
                RECT rcEmpty = {15, 60, rc.right - 15, 110};
                DrawTextW(memDC,
                    L"Записей пока нет.\nНажмите на иконку в трее, чтобы добавить.",
                    -1, &rcEmpty, DT_LEFT | DT_WORDBREAK);
            }

            SetTextColor(memDC, RGB(150, 150, 170));
            SelectObject(memDC, hFontHint);
            RECT rcHint = {15, rc.bottom - 25, rc.right - 15, rc.bottom - 5};
            DrawTextW(memDC, L"ЛКМ — открыть редактор  •  Ctrl+Alt+D",
                      -1, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            DeleteObject(hFontTitle);
            DeleteObject(hFontText);
            DeleteObject(hFontHint);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            ShowWindow(hMainWnd, SW_SHOW);
            SetForegroundWindow(hMainWnd);
            AnimateWindow(hPopupWnd, 120, AW_BLEND | AW_HIDE);
            showingPopup = 0;
            return 0;
        }

        case WM_KILLFOCUS:
            if (showingPopup) {
                AnimateWindow(hWnd, 120, AW_BLEND | AW_HIDE);
                showingPopup = 0;
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void createPopupWindow() {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = PopupProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"DiaryPopup";
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    RegisterClassW(&wc);

    hPopupWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"DiaryPopup", L"Виджет",
        WS_POPUP,
        0, 0, 380, 260,
        NULL, NULL, hInst, NULL);

    SetLayeredWindowAttributes(hPopupWnd, 0, 217, LWA_ALPHA);

    HRGN rgn = createRoundedRegion(380, 260, 20);
    SetWindowRgn(hPopupWnd, rgn, TRUE);

    #ifndef DWMWA_WINDOW_CORNER_PREFERENCE
    #define DWMWA_WINDOW_CORNER_PREFERENCE 33
    #endif
    #ifndef DWMWCP_ROUND
    #define DWMWCP_ROUND 2
    #endif
    int pref = DWMWCP_ROUND;
    typedef HRESULT (WINAPI *SetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (hDwm) {
        SetAttrFn fn = (SetAttrFn)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (fn) fn(hPopupWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    }
}

void showPopupNearTray() {
    if (showingPopup) {
        AnimateWindow(hPopupWnd, 150, AW_BLEND | AW_HIDE);
        showingPopup = 0;
        return;
    }

    POINT pt;
    GetCursorPos(&pt);

    int w = 380, h = 260;
    int x = pt.x - w + 20;
    int y = pt.y - h - 10;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    if (x < workArea.left) x = workArea.left + 10;
    if (y < workArea.top) y = pt.y + 20;
    if (x + w > workArea.right) x = workArea.right - w - 10;

    SetWindowPos(hPopupWnd, HWND_TOPMOST, x, y, w, h,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);

    InvalidateRect(hPopupWnd, NULL, TRUE);

    SetLayeredWindowAttributes(hPopupWnd, 0, 0, LWA_ALPHA);
    for (int a = 0; a <= 217; a += 17) {
        SetLayeredWindowAttributes(hPopupWnd, 0, a, LWA_ALPHA);
        Sleep(8);
    }

    SetForegroundWindow(hPopupWnd);
    showingPopup = 1;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"Текст записи:",
                WS_CHILD | WS_VISIBLE,
                15, 15, 120, 20, hWnd, NULL, hInst, NULL);

            hEditText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                15, 40, 440, 28, hWnd, (HMENU)ID_EDIT_TEXT, hInst, NULL);

            CreateWindowW(L"STATIC", L"Приоритет (1-3):",
                WS_CHILD | WS_VISIBLE,
                15, 78, 130, 20, hWnd, NULL, hInst, NULL);

            hEditPrio = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"2",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                150, 75, 60, 25, hWnd, (HMENU)ID_EDIT_PRIORITY, hInst, NULL);

            CreateWindowW(L"BUTTON", L"Добавить",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                230, 73, 110, 30, hWnd, (HMENU)ID_BTN_ADD, hInst, NULL);

            CreateWindowW(L"BUTTON", L"Удалить",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                350, 73, 105, 30, hWnd, (HMENU)ID_BTN_DELETE, hInst, NULL);

            hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
                15, 115, 440, 300, hWnd, (HMENU)ID_LIST, hInst, NULL);

            CreateWindowW(L"BUTTON", L"Свернуть в трей",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                15, 425, 160, 30, hWnd, (HMENU)ID_BTN_CLOSE, hInst, NULL);

            loadFromFile();
            refreshList();
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_BTN_ADD:    addZapis(hWnd);    break;
                case ID_BTN_DELETE: deleteZapis(hWnd); break;
                case ID_BTN_CLOSE:  ShowWindow(hWnd, SW_HIDE); break;
            }
            return 0;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONUP) {
                showPopupNearTray();
            } else if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();

                AppendMenuW(hMenu, MF_STRING, IDM_OPEN,   L"Открыть редактор");
                AppendMenuW(hMenu, MF_STRING, IDM_WIDGET, L"Показать виджет");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu,
                    MF_STRING | (isAutostartEnabled() ? MF_CHECKED : 0),
                    IDM_AUTOSTART, L"Запускать с Windows");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Выход");

                SetForegroundWindow(hWnd);
                int cmd = TrackPopupMenu(hMenu,
                    TPM_RETURNCMD | TPM_RIGHTBUTTON,
                    pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);

                switch (cmd) {
                    case IDM_OPEN:
                        ShowWindow(hWnd, SW_SHOW);
                        SetForegroundWindow(hWnd);
                        break;
                    case IDM_WIDGET:
                        showPopupNearTray();
                        break;
                    case IDM_AUTOSTART:
                        setAutostart(!isAutostartEnabled());
                        break;
                    case IDM_EXIT:
                        DestroyWindow(hWnd);
                        break;
                }
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == ID_HOTKEY) {
                if (IsWindowVisible(hWnd)) ShowWindow(hWnd, SW_HIDE);
                else {
                    ShowWindow(hWnd, SW_SHOW);
                    SetForegroundWindow(hWnd);
                }
            }
            return 0;
        }

        case WM_TIMER: {
            wchar_t today[11];
            getCurrentDate(today);
            for (int i = 0; i < count; i++) {
                if (wcscmp(diary[i].date, today) == 0 && diary[i].priority == 3) {
                    showPopupNearTray();
                    break;
                }
            }
            return 0;
        }

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            saveToFile();
            Shell_NotifyIconW(NIM_DELETE, &nid);
            UnregisterHotKey(hWnd, ID_HOTKEY);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR pCmdLine, int nCmdShow) {
    hInst = hInstance;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    hIconColor = createDiaryIcon(32, FALSE);
    hIconGray  = createDiaryIcon(32, TRUE);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DiaryMainWnd";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = hIconColor;
    RegisterClassW(&wc);

    hMainWnd = CreateWindowExW(
        0, L"DiaryMainWnd", L"Ежедневник",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 510,
        NULL, NULL, hInstance, NULL);

    if (!hMainWnd) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    createPopupWindow();

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hMainWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(nid.szTip, L"Ежедневник — клик для виджета");
    Shell_NotifyIconW(NIM_ADD, &nid);

    RegisterHotKey(hMainWnd, ID_HOTKEY, MOD_CONTROL | MOD_ALT, 'D');
    SetTimer(hMainWnd, ID_TIMER, 30 * 60 * 1000, NULL);

    ShowWindow(hMainWnd, SW_HIDE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hIconColor) DestroyIcon(hIconColor);
    if (hIconGray)  DestroyIcon(hIconGray);
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}
