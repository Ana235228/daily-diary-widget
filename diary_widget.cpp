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

// ===== Палитра "aesthetic planner" =====
#define CLR_BG          RGB(46, 58, 51)
#define CLR_SURFACE     RGB(58, 74, 66)
#define CLR_SURFACE_HOV RGB(74, 93, 83)
#define CLR_INPUT       RGB(38, 48, 43)
#define CLR_TEXT        RGB(237, 228, 211)
#define CLR_MUTED       RGB(168, 163, 148)
#define CLR_DIVIDER     RGB(79, 94, 85)
#define CLR_ACCENT      RGB(201, 123, 90)
#define CLR_ACCENT2     RGB(212, 168, 92)
#define CLR_ACCENT3     RGB(122, 155, 118)

#define WIN_W            560
#define WIN_H            680
#define UI_PAD           28
#define UI_HEADER_H      90
#define UI_INPUT_Y       110
#define UI_INPUT_H       52
#define UI_BTN_Y         174
#define UI_BTN_H         42
#define UI_BTN_GAP       10
#define UI_PROG_Y        232
#define UI_LIST_TOP      288
#define UI_LIST_BOTTOM   660
#define UI_LIST_H        (UI_LIST_BOTTOM - UI_LIST_TOP)
#define UI_CARD_H        52
#define UI_CARD_GAP      8
#define UI_ROW           (UI_CARD_H + UI_CARD_GAP)
#define ID_TIMER_CARET   2

static const COLORREF CLR_PRIO[3] = {
    RGB(122, 155, 118),
    RGB(212, 168, 92),
    RGB(201, 123, 90)
};

static const bool DATE_AT_RIGHT = false;

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
    bool done;
} Zapis;

typedef struct {
    int id;
    wchar_t date[11];
    wchar_t time[6];
    wchar_t text[MAX_DLINA];
    int priority;
} ZapisV1;

Zapis diary[MAX_ZAPISEY];
int count = 0;

// ===== Состояние главного окна =====
enum { H_NONE = 0, H_ADD, H_DEL, H_HIDE, H_INPUT, H_PRIO, H_CHECK, H_CARD };
struct Hit { int kind; int idx; };

static Hit  g_hover    = { H_NONE, -1 };
static wchar_t g_input[MAX_DLINA] = L"";
static int  g_inputLen = 0;
static int  g_newPrio  = 2;
static int  g_scroll   = 0;
static int  g_selected = -1;
static bool g_caretOn  = true;
static bool g_tracking = false;

static struct {
    HFONT title, seg9, seg10, seg11, seg11i, seg11s, btn, geo16;
} gF = {0};

static int maxScroll() {
    int content = count > 0 ? count * UI_ROW - UI_CARD_GAP : 0;
    int m = content - UI_LIST_H;
    return m > 0 ? m : 0;
}
static void clampScroll() {
    int m = maxScroll();
    if (g_scroll > m) g_scroll = m;
    if (g_scroll < 0) g_scroll = 0;
}

HINSTANCE hInst;
HWND hMainWnd = NULL;
HWND hPopupWnd = NULL;
NOTIFYICONDATAW nid;
int showingPopup = 0;
HICON hIconColor = NULL;
HICON hIconGray  = NULL;
ULONG_PTR gdiplusToken = 0;

const wchar_t *FILE_NAME = L"diary.dat";
const wchar_t *REG_RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";





static HFONT g_fTitle = NULL, g_fText = NULL, g_fDate = NULL;

static HFONT makeFont(const wchar_t *face, int pt, BOOL italic, int weight) {
    HDC dc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(NULL, dc);
    return CreateFontW(h, 0, 0, 0, weight, italic, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void fillRectColor(HDC dc, int l, int t, int r, int b, COLORREF c) {
    RECT rc = {l, t, r, b};
    HBRUSH br = CreateSolidBrush(c);
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

static void drawDot(HDC dc, int cx, int cy, int r, COLORREF c) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush br(Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c)));
    g.FillEllipse(&br, (Gdiplus::REAL)(cx - r), (Gdiplus::REAL)(cy - r),
                  (Gdiplus::REAL)(2 * r), (Gdiplus::REAL)(2 * r));
}

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

    Color coverColor  = gray ? Color(255, 120, 126, 122) : Color(255, 76, 98, 87);
    Color coverDark   = gray ? Color(255, 84, 90, 87)    : Color(255, 46, 60, 53);
    Color edgeColor   = gray ? Color(200, 200, 198, 192) : Color(220, 237, 228, 211);
    Color pagesColor  = gray ? Color(255, 214, 212, 206) : Color(255, 237, 228, 211);
    Color lineColor   = gray ? Color(190, 150, 150, 146) : Color(200, 168, 163, 148);
    Color ribbonColor = gray ? Color(255, 150, 148, 140) : Color(255, 201, 123, 90);

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
    Pen edgePen(edgeColor, s * 0.03f);
    g.DrawPath(&edgePen, &coverPath);

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

    Pen linePen(lineColor, s * 0.02f);
    for (int i = 0; i < 3; i++) {
        float ly = pageTop + h * (0.22f + i * 0.18f);
        g.DrawLine(&linePen, pageLeft + w * 0.05f, ly,
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

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    int n = 0;
    if (fread(&n, sizeof(int), 1, f) != 1 || n < 0) { fclose(f); return; }
    if (n > MAX_ZAPISEY) n = MAX_ZAPISEY;

    long payload = fileSize - (long)sizeof(int);
    bool migrated = false;

    if (n > 0 && payload < (long)(n * sizeof(Zapis)) &&
                 payload >= (long)(n * sizeof(ZapisV1))) {
        ZapisV1 old;
        int i = 0;
        for (; i < n; i++) {
            if (fread(&old, sizeof(old), 1, f) != 1) break;
            memset(&diary[i], 0, sizeof(Zapis));
            diary[i].id = old.id;
            wcscpy(diary[i].date, old.date);
            wcscpy(diary[i].time, old.time);
            wcscpy(diary[i].text, old.text);
            diary[i].priority = old.priority;
            diary[i].done = false;
        }
        count = i;
        migrated = true;
    } else {
        count = (int)fread(diary, sizeof(Zapis), n, f);
    }
    fclose(f);

    for (int i = 0; i < count; i++) {
        diary[i].date[10] = 0;
        diary[i].time[5] = 0;
        diary[i].text[MAX_DLINA - 1] = 0;
        if (diary[i].priority < 1 || diary[i].priority > 3) diary[i].priority = 2;
    }
    if (migrated) saveToFile();
}

void refreshList() {
    if (g_selected >= count) g_selected = -1;
    clampScroll();
    if (hMainWnd) InvalidateRect(hMainWnd, NULL, FALSE);
}

void addZapis(HWND hWnd) {
    if (count >= MAX_ZAPISEY) {
        MessageBox(hWnd, L"Ежедневник заполнен!", L"Ошибка", MB_OK | MB_ICONWARNING);
        return;
    }
    if (g_inputLen == 0) {
        MessageBox(hWnd, L"Введите текст записи!", L"Внимание", MB_OK | MB_ICONINFORMATION);
        return;
    }

    Zapis *z = &diary[count];
    memset(z, 0, sizeof(Zapis));
    z->id = (count == 0) ? 1 : diary[count - 1].id + 1;
    getCurrentDate(z->date);
    getCurrentTime(z->time);
    wcscpy(z->text, g_input);
    z->priority = (g_newPrio >= 1 && g_newPrio <= 3) ? g_newPrio : 2;
    z->done = false;

    count++;
    saveToFile();

    g_input[0] = 0;
    g_inputLen = 0;
    g_scroll = 0;
    g_selected = -1;
    refreshList();
}

void deleteZapis(HWND hWnd) {
    int sel = g_selected;
    if (sel < 0 || sel >= count) {
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
    g_selected = -1;
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
        case WM_CREATE:
            g_fTitle = makeFont(L"Georgia",  24, TRUE,  FW_NORMAL);
            g_fText  = makeFont(L"Segoe UI", 10, FALSE, FW_NORMAL);
            g_fDate  = makeFont(L"Segoe UI",  8, FALSE, FW_NORMAL);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
            HGDIOBJ oldFont = GetCurrentObject(memDC, OBJ_FONT);

            const int PAD = 20, ROW_H = 24, DATE_W = 70;

            fillRectColor(memDC, 0, 0, rc.right, rc.bottom, CLR_SURFACE);
            SetBkMode(memDC, TRANSPARENT);

            SelectObject(memDC, g_fTitle);
            SetTextColor(memDC, CLR_TEXT);
            RECT rcTitle = {PAD, 8, rc.right - PAD, 50};
            DrawTextW(memDC, L"planner", -1, &rcTitle,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            fillRectColor(memDC, PAD, 52, rc.right - PAD, 53, CLR_DIVIDER);

            int y = 58;
            int shown = 0;
            int start = count > 7 ? count - 7 : 0;

            for (int i = start; i < count && shown < 7; i++, shown++) {
                int p = diary[i].priority;
                if (p < 1 || p > 3) p = 2;

                drawDot(memDC, PAD + 4, y + ROW_H / 2, 4, CLR_PRIO[p - 1]);

                int textL = PAD + 18;
                int textR = rc.right - PAD;
                int rowB  = y + ROW_H - 1;
                RECT rcDate, rcText;
                UINT dateAlign;

                if (DATE_AT_RIGHT) {
                    rcDate = {textR - DATE_W, y, textR, rowB};
                    rcText = {textL, y, textR - DATE_W - 8, rowB};
                    dateAlign = DT_RIGHT;
                } else {
                    rcDate = {textL, y, textL + DATE_W, rowB};
                    rcText = {textL + DATE_W + 6, y, textR, rowB};
                    dateAlign = DT_LEFT;
                }

                SelectObject(memDC, g_fDate);
                SetTextColor(memDC, CLR_MUTED);
                DrawTextW(memDC, diary[i].date, -1, &rcDate,
                          dateAlign | DT_VCENTER | DT_SINGLELINE);

                SelectObject(memDC, g_fText);
                SetTextColor(memDC, CLR_TEXT);
                DrawTextW(memDC, diary[i].text, -1, &rcText,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                if (i < count - 1 && shown < 6)
                    fillRectColor(memDC, PAD, y + ROW_H - 1,
                                  rc.right - PAD, y + ROW_H, CLR_DIVIDER);

                y += ROW_H;
            }

            if (count == 0) {
                SelectObject(memDC, g_fText);
                SetTextColor(memDC, CLR_MUTED);
                RECT rcEmpty = {PAD, 70, rc.right - PAD, 130};
                DrawTextW(memDC,
                    L"Записей пока нет.\nНажмите на иконку в трее, чтобы добавить.",
                    -1, &rcEmpty, DT_LEFT | DT_WORDBREAK);
            }

            SelectObject(memDC, g_fDate);
            SetTextColor(memDC, CLR_MUTED);
            RECT rcHint = {PAD, rc.bottom - 25, rc.right - PAD, rc.bottom - 5};
            DrawTextW(memDC, L"ЛКМ — открыть редактор  •  Ctrl+Alt+D",
                      -1, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldFont);
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
            if (g_fTitle) { DeleteObject(g_fTitle); g_fTitle = NULL; }
            if (g_fText)  { DeleteObject(g_fText);  g_fText  = NULL; }
            if (g_fDate)  { DeleteObject(g_fDate);  g_fDate  = NULL; }
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

    HRGN rgn = createRoundedRegion(380, 260, 16);
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

// ===== Вспомогательные =====
static Gdiplus::Color gc(COLORREF c, BYTE a = 255) {
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}
static COLORREF mixColor(COLORREF a, COLORREF b, int pct) {
    int r = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * pct / 100;
    int g = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * pct / 100;
    int bl = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * pct / 100;
    return RGB(r, g, bl);
}
static HFONT makeFontStrike(const wchar_t *face, int pt) {
    HDC dc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(NULL, dc);
    return CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, TRUE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}
static void roundRectPath(Gdiplus::GraphicsPath &p, float x, float y,
                          float w, float h, float r) {
    float d = r * 2;
    p.AddArc(x, y, d, d, 180.0f, 90.0f);
    p.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    p.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    p.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    p.CloseFigure();
}
static void gFillRound(Gdiplus::Graphics &g, float x, float y, float w, float h,
                       float r, COLORREF c) {
    Gdiplus::GraphicsPath p;
    roundRectPath(p, x, y, w, h, r);
    Gdiplus::SolidBrush b(gc(c));
    g.FillPath(&b, &p);
}
static void gStrokeRound(Gdiplus::Graphics &g, float x, float y, float w, float h,
                         float r, COLORREF c, float pw) {
    Gdiplus::GraphicsPath p;
    roundRectPath(p, x, y, w, h, r);
    Gdiplus::Pen pen(gc(c), pw);
    g.DrawPath(&pen, &p);
}
static void gDot(Gdiplus::Graphics &g, float cx, float cy, float r, COLORREF c) {
    Gdiplus::SolidBrush b(gc(c));
    g.FillEllipse(&b, cx - r, cy - r, r * 2, r * 2);
}
static void getHeaderDate(wchar_t *buf, int n) {
    static const wchar_t *wd[] = { L"Воскресенье", L"Понедельник", L"Вторник",
        L"Среда", L"Четверг", L"Пятница", L"Суббота" };
    static const wchar_t *mo[] = { L"января", L"февраля", L"марта", L"апреля",
        L"мая", L"июня", L"июля", L"августа", L"сентября", L"октября",
        L"ноября", L"декабря" };
    time_t t = time(NULL);
    struct tm *tmi = localtime(&t);
    swprintf(buf, n, L"%ls, %d %ls", wd[tmi->tm_wday], tmi->tm_mday, mo[tmi->tm_mon]);
}
static RECT rcInput() {
    RECT r = { UI_PAD, UI_INPUT_Y, WIN_W - UI_PAD, UI_INPUT_Y + UI_INPUT_H };
    return r;
}
static RECT rcBtn(int i) {
    static const int w[3] = { 130, 110, 120 };
    int x = UI_PAD;
    for (int k = 0; k < i; k++) x += w[k] + UI_BTN_GAP;
    RECT r = { x, UI_BTN_Y, x + w[i], UI_BTN_Y + UI_BTN_H };
    return r;
}
static float prioCx(int k) {
    return (float)(WIN_W - UI_PAD - 20 - (2 - k) * 22);
}
static bool ptIn(const RECT &r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}
static Hit hitTest(int x, int y) {
    Hit h = { H_NONE, -1 };
    for (int b = 0; b < 3; b++) {
        RECT r = rcBtn(b);
        if (ptIn(r, x, y)) { h.kind = H_ADD + b; return h; }
    }
    RECT ri = rcInput();
    if (ptIn(ri, x, y)) {
        float cy = ri.top + UI_INPUT_H / 2.0f;
        for (int k = 0; k < 3; k++) {
            float dx = x - prioCx(k), dy = y - cy;
            if (dx * dx + dy * dy <= 11.0f * 11.0f) { h.kind = H_PRIO; h.idx = k + 1; return h; }
        }
        h.kind = H_INPUT;
        return h;
    }
    if (y >= UI_LIST_TOP && y < UI_LIST_BOTTOM && x >= UI_PAD && x < WIN_W - UI_PAD) {
        int rel = y - UI_LIST_TOP + g_scroll;
        int r = rel / UI_ROW, within = rel % UI_ROW;
        if (within < UI_CARD_H && r < count) {
            int idx = count - 1 - r;
            float cx = UI_PAD + 26.0f;
            float cy = (float)(UI_LIST_TOP + r * UI_ROW - g_scroll + UI_CARD_H / 2);
            float dx = x - cx, dy = y - cy;
            h.idx = idx;
            h.kind = (dx * dx + dy * dy <= 14.0f * 14.0f) ? H_CHECK : H_CARD;
        }
    }
    return h;
}
static void updateHover(HWND hWnd, int x, int y) {
    Hit h = hitTest(x, y);
    if (h.kind != g_hover.kind || h.idx != g_hover.idx) {
        g_hover = h;
        InvalidateRect(hWnd, NULL, FALSE);
    }
}
static void invalidateInput(HWND hWnd) {
    RECT ri = rcInput();
    InvalidateRect(hWnd, &ri, FALSE);
}

// ===== Отрисовка секций =====
static void paintHeader(HDC dc, Gdiplus::Graphics &g) {
    for (int k = 0; k < 3; k++)
        gDot(g, (float)(WIN_W - UI_PAD - 4 - (2 - k) * 16), 58.0f, 4, CLR_PRIO[k]);
    g.Flush(Gdiplus::FlushIntentionSync);
    wchar_t d[64];
    getHeaderDate(d, 64);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT r1 = { UI_PAD, 14, 400, 34 };
    DrawTextW(dc, d, -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, gF.title);
    SetTextColor(dc, CLR_TEXT);
    RECT r2 = { UI_PAD, 32, 400, 86 };
    DrawTextW(dc, L"planner", -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    fillRectColor(dc, 0, UI_HEADER_H, WIN_W, UI_HEADER_H + 1, CLR_DIVIDER);
}
static void paintInput(HDC dc, Gdiplus::Graphics &g) {
    RECT ri = rcInput();
    float w = (float)(ri.right - ri.left), h = (float)(ri.bottom - ri.top);
    bool hov = (g_hover.kind == H_INPUT || g_hover.kind == H_PRIO);
    gFillRound(g, (float)ri.left, (float)ri.top, w, h, 12, CLR_INPUT);
    gStrokeRound(g, ri.left + 0.5f, ri.top + 0.5f, w - 1, h - 1, 12,
                 hov ? CLR_MUTED : CLR_DIVIDER, 1.0f);
    float cy = ri.top + h / 2;
    for (int k = 0; k < 3; k++) {
        float cx = prioCx(k);
        gDot(g, cx, cy, 5, CLR_PRIO[k]);
        if (g_newPrio == k + 1) {
            Gdiplus::Pen ring(gc(CLR_TEXT), 1.5f);
            g.DrawEllipse(&ring, cx - 9, cy - 9, 18.0f, 18.0f);
        } else if (g_hover.kind == H_PRIO && g_hover.idx == k + 1) {
            Gdiplus::Pen ring(gc(CLR_MUTED), 1.0f);
            g.DrawEllipse(&ring, cx - 9, cy - 9, 18.0f, 18.0f);
        }
    }
    g.Flush(Gdiplus::FlushIntentionSync);
    SetBkMode(dc, TRANSPARENT);
    int textL = ri.left + 18;
    int avail = (int)prioCx(0) - 11 - 12 - textL;
    RECT rt = { textL, ri.top, textL + avail, ri.bottom };
    int caretX = textL;
    if (g_inputLen == 0) {
        SelectObject(dc, gF.seg11i);
        SetTextColor(dc, CLR_MUTED);
        DrawTextW(dc, L"Что нужно сделать...", -1, &rt,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SelectObject(dc, gF.seg11);
        SetTextColor(dc, CLR_TEXT);
        SIZE sz;
        GetTextExtentPoint32W(dc, g_input, g_inputLen, &sz);
        bool overflow = sz.cx > avail;
        DrawTextW(dc, g_input, g_inputLen, &rt,
                  (overflow ? DT_RIGHT : DT_LEFT) | DT_VCENTER | DT_SINGLELINE);
        caretX = textL + (overflow ? avail : sz.cx) + 1;
    }
    if (g_caretOn) {
        int mid = ri.top + UI_INPUT_H / 2;
        fillRectColor(dc, caretX, mid - 11, caretX + 2, mid + 11, CLR_TEXT);
    }
}
static void paintButtons(HDC dc, Gdiplus::Graphics &g) {
    static const wchar_t *lbl[3] = { L"Добавить", L"Удалить", L"Свернуть" };
    COLORREF fg[3] = { CLR_BG, g_selected >= 0 ? CLR_TEXT : CLR_MUTED, CLR_TEXT };
    SelectObject(dc, gF.btn);
    SIZE sz;
    GetTextExtentPoint32W(dc, lbl[0], (int)wcslen(lbl[0]), &sz);
    RECT ra = rcBtn(0);
    int group = 12 + 8 + sz.cx;
    int gx = ra.left + ((ra.right - ra.left) - group) / 2;
    for (int b = 0; b < 3; b++) {
        RECT r = rcBtn(b);
        bool hov = (g_hover.kind == H_ADD + b);
        COLORREF fill;
        if (b == 0) fill = hov ? mixColor(CLR_ACCENT, RGB(255, 255, 255), 14) : CLR_ACCENT;
        else        fill = hov ? CLR_SURFACE_HOV : CLR_SURFACE;
        gFillRound(g, (float)r.left, (float)r.top,
                   (float)(r.right - r.left), (float)(r.bottom - r.top), 12, fill);
        if (b == 0) {
            float cx = gx + 6.0f, cy = r.top + UI_BTN_H / 2.0f;
            Gdiplus::Pen pen(gc(CLR_BG), 2.0f);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&pen, cx - 5.0f, cy, cx + 5.0f, cy);
            g.DrawLine(&pen, cx, cy - 5.0f, cx, cy + 5.0f);
        }
    }
    g.Flush(Gdiplus::FlushIntentionSync);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.btn);
    for (int b = 0; b < 3; b++) {
        RECT r = rcBtn(b);
        SetTextColor(dc, fg[b]);
        if (b == 0) {
            r.left = gx + 20;
            DrawTextW(dc, lbl[b], -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {
            DrawTextW(dc, lbl[b], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}
static void paintProgress(HDC dc, Gdiplus::Graphics &g) {
    wchar_t today[11];
    getCurrentDate(today);
    int total = 0, done = 0;
    for (int i = 0; i < count; i++) {
        if (wcscmp(diary[i].date, today) == 0) {
            total++;
            if (diary[i].done) done++;
        }
    }
    float x = (float)UI_PAD, y = (float)(UI_PROG_Y + 30);
    float w = (float)(WIN_W - UI_PAD * 2);
    gFillRound(g, x, y, w, 4, 2, CLR_DIVIDER);
    float fw = total > 0 ? w * done / total : 0.0f;
    if (fw >= 4.0f) {
        Gdiplus::LinearGradientBrush br(Gdiplus::PointF(x, 0), Gdiplus::PointF(x + w, 0),
                                        gc(CLR_ACCENT3), gc(CLR_ACCENT2));
        Gdiplus::GraphicsPath p;
        roundRectPath(p, x, y, fw, 4, 2);
        g.FillPath(&br, &p);
    }
    g.Flush(Gdiplus::FlushIntentionSync);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.seg11);
    SetTextColor(dc, CLR_TEXT);
    RECT r1 = { UI_PAD, UI_PROG_Y, 300, UI_PROG_Y + 24 };
    DrawTextW(dc, L"Сегодня", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wchar_t buf[32];
    swprintf(buf, 32, L"%d из %d", done, total);
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT r2 = { 260, UI_PROG_Y, WIN_W - UI_PAD, UI_PROG_Y + 24 };
    DrawTextW(dc, buf, -1, &r2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}
static void paintEmpty(HDC dc, Gdiplus::Graphics &g) {
    using namespace Gdiplus;
    float ox = WIN_W / 2.0f, oy = 410.0f;
    Pen pen(gc(CLR_MUTED, 210), 2.0f);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
    GraphicsPath cup;
    cup.AddLine(ox - 34.0f, oy, ox + 34.0f, oy);
    cup.AddLine(ox + 34.0f, oy, ox + 34.0f, oy + 36.0f);
    cup.AddArc(ox - 6.0f, oy + 16.0f, 40.0f, 40.0f, 0.0f, 90.0f);
    cup.AddLine(ox + 14.0f, oy + 56.0f, ox - 14.0f, oy + 56.0f);
    cup.AddArc(ox - 34.0f, oy + 16.0f, 40.0f, 40.0f, 90.0f, 90.0f);
    cup.CloseFigure();
    g.DrawPath(&pen, &cup);
    g.DrawArc(&pen, ox + 22.0f, oy + 10.0f, 28.0f, 26.0f, -90.0f, 180.0f);
    g.DrawLine(&pen, ox - 52.0f, oy + 66.0f, ox + 52.0f, oy + 66.0f);
    static const float sx[3] = { -14.0f, 0.0f, 14.0f };
    static const float sh[3] = { 26.0f, 36.0f, 26.0f };
    for (int i = 0; i < 3; i++) {
        float x = ox + sx[i];
        g.DrawBezier(&pen, x, oy - 8.0f, x + 8.0f, oy - 8.0f - sh[i] * 0.35f,
                     x - 8.0f, oy - 8.0f - sh[i] * 0.7f, x, oy - 8.0f - sh[i]);
    }
    g.Flush(FlushIntentionSync);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, CLR_MUTED);
    SelectObject(dc, gF.geo16);
    RECT r1 = { 0, (int)oy + 90, WIN_W, (int)oy + 122 };
    DrawTextW(dc, L"Тут пока пусто", -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, gF.seg10);
    RECT r2 = { 0, (int)oy + 124, WIN_W, (int)oy + 146 };
    DrawTextW(dc, L"Добавь первую запись", -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
static void paintList(HDC dc, Gdiplus::Graphics &g) {
    if (count == 0) { paintEmpty(dc, g); return; }
    clampScroll();
    g.SetClip(Gdiplus::Rect(0, UI_LIST_TOP, WIN_W, UI_LIST_H));
    int first = g_scroll / UI_ROW;
    int last = (g_scroll + UI_LIST_H) / UI_ROW;
    if (last > count - 1) last = count - 1;
    float cw = (float)(WIN_W - UI_PAD * 2);
    for (int r = first; r <= last; r++) {
        int idx = count - 1 - r;
        float y = (float)(UI_LIST_TOP + r * UI_ROW - g_scroll);
        bool sel = (g_selected == idx);
        bool hov = (g_hover.idx == idx &&
                    (g_hover.kind == H_CARD || g_hover.kind == H_CHECK));
        bool hchk = (g_hover.kind == H_CHECK && g_hover.idx == idx);
        gFillRound(g, (float)UI_PAD, y, cw, (float)UI_CARD_H, 12,
                   hov ? CLR_SURFACE_HOV : CLR_SURFACE);
        if (sel)
            gStrokeRound(g, UI_PAD + 0.5f, y + 0.5f, cw - 1, UI_CARD_H - 1.0f, 12,
                         CLR_MUTED, 1.0f);
        float cx = UI_PAD + 26.0f, cy = y + UI_CARD_H / 2.0f;
        if (diary[idx].done) {
            gDot(g, cx, cy, 10, hchk ? mixColor(CLR_ACCENT3, RGB(255, 255, 255), 15)
                                     : CLR_ACCENT3);
            Gdiplus::Pen chk(Gdiplus::Color(255, 255, 255, 255), 2.0f);
            chk.SetStartCap(Gdiplus::LineCapRound);
            chk.SetEndCap(Gdiplus::LineCapRound);
            chk.SetLineJoin(Gdiplus::LineJoinRound);
            Gdiplus::PointF pts[3] = {
                Gdiplus::PointF(cx - 4.5f, cy + 0.5f),
                Gdiplus::PointF(cx - 1.0f, cy + 4.0f),
                Gdiplus::PointF(cx + 4.5f, cy - 3.5f) };
            g.DrawLines(&chk, pts, 3);
        } else {
            Gdiplus::Pen ring(gc(hchk ? CLR_ACCENT3 : CLR_MUTED), 1.5f);
            g.DrawEllipse(&ring, cx - 9.25f, cy - 9.25f, 18.5f, 18.5f);
        }
        int p = diary[idx].priority;
        if (p < 1 || p > 3) p = 2;
        gDot(g, cx + 10.0f + 14.0f, cy, 4, CLR_PRIO[p - 1]);
    }
    int ms = maxScroll();
    if (ms > 0) {
        int content = count * UI_ROW - UI_CARD_GAP;
        float th = (float)UI_LIST_H * UI_LIST_H / content;
        if (th < 24.0f) th = 24.0f;
        float ty = UI_LIST_TOP + (UI_LIST_H - th) * g_scroll / ms;
        gFillRound(g, (float)(WIN_W - 12), ty, 3.0f, th, 1.5f, CLR_DIVIDER);
    }
    g.Flush(Gdiplus::FlushIntentionSync);
    int saved = SaveDC(dc);
    IntersectClipRect(dc, 0, UI_LIST_TOP, WIN_W, UI_LIST_BOTTOM);
    SetBkMode(dc, TRANSPARENT);
    for (int r = first; r <= last; r++) {
        int idx = count - 1 - r;
        int y = UI_LIST_TOP + r * UI_ROW - g_scroll;
        bool done = diary[idx].done;
        SelectObject(dc, gF.seg9);
        SetTextColor(dc, CLR_MUTED);
        RECT rd = { 94, y, 164, y + UI_CARD_H };
        DrawTextW(dc, diary[idx].date, -1, &rd, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, done ? gF.seg11s : gF.seg11);
        SetTextColor(dc, done ? CLR_MUTED : CLR_TEXT);
        RECT rt = { 172, y, WIN_W - UI_PAD - 16, y + UI_CARD_H };
        DrawTextW(dc, diary[idx].text, -1, &rt,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    RestoreDC(dc, saved);
    g.ResetClip();
}
static void pasteClipboard(HWND hWnd) {
    if (!OpenClipboard(hWnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t *s = (const wchar_t*)GlobalLock(h);
        if (s) {
            for (; *s && g_inputLen < MAX_DLINA - 1; s++)
                if (*s >= 32) g_input[g_inputLen++] = *s;
            g_input[g_inputLen] = 0;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}
static void applyMainWindowChrome(HWND hWnd) {
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;
    typedef HRESULT (WINAPI *SetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
    SetAttrFn fn = (SetAttrFn)GetProcAddress(hDwm, "DwmSetWindowAttribute");
    if (fn) {
        BOOL dark = TRUE;          fn(hWnd, 20, &dark, sizeof(dark));
        COLORREF brd = CLR_DIVIDER; fn(hWnd, 34, &brd,  sizeof(brd));
        COLORREF cap = CLR_BG;      fn(hWnd, 35, &cap,  sizeof(cap));
        COLORREF txt = CLR_TEXT;    fn(hWnd, 36, &txt,  sizeof(txt));
    }
    FreeLibrary(hDwm);
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            gF.title  = makeFont(L"Georgia",  30, TRUE,  FW_NORMAL);
            gF.geo16  = makeFont(L"Georgia",  16, TRUE,  FW_NORMAL);
            gF.seg9   = makeFont(L"Segoe UI",  9, FALSE, FW_NORMAL);
            gF.seg10  = makeFont(L"Segoe UI", 10, FALSE, FW_NORMAL);
            gF.seg11  = makeFont(L"Segoe UI", 11, FALSE, FW_NORMAL);
            gF.seg11i = makeFont(L"Segoe UI", 11, TRUE,  FW_NORMAL);
            gF.seg11s = makeFontStrike(L"Segoe UI", 11);
            gF.btn    = makeFont(L"Segoe UI", 10, FALSE, FW_SEMIBOLD);
            SetTimer(hWnd, ID_TIMER_CARET, 500, NULL);
            loadFromFile();
            refreshList();
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, WIN_W, WIN_H);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
            HGDIOBJ oldFont = GetCurrentObject(memDC, OBJ_FONT);
            fillRectColor(memDC, 0, 0, WIN_W, WIN_H, CLR_BG);
            SetBkMode(memDC, TRANSPARENT);
            {
                Gdiplus::Graphics g(memDC);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                paintHeader(memDC, g);
                paintInput(memDC, g);
                paintButtons(memDC, g);
                paintProgress(memDC, g);
                paintList(memDC, g);
                g.Flush(Gdiplus::FlushIntentionSync);
            }
            BitBlt(hdc, 0, 0, WIN_W, WIN_H, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldFont);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);
                Hit h = hitTest(pt.x, pt.y);
                SetCursor(LoadCursor(NULL, h.kind == H_NONE ? IDC_ARROW
                                          : (h.kind == H_INPUT ? IDC_IBEAM : IDC_HAND)));
                return TRUE;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (!g_tracking) {
                TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&t);
                g_tracking = true;
            }
            updateHover(hWnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
            return 0;
        }
        case WM_MOUSELEAVE:
            g_tracking = false;
            g_hover.kind = H_NONE;
            g_hover.idx = -1;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(hWnd);
            Hit h = hitTest((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
            switch (h.kind) {
                case H_ADD:   addZapis(hWnd);    break;
                case H_DEL:   deleteZapis(hWnd); break;
                case H_HIDE:  ShowWindow(hWnd, SW_HIDE); break;
                case H_PRIO:  g_newPrio = h.idx; break;
                case H_INPUT: g_caretOn = true;  break;
                case H_CHECK:
                    diary[h.idx].done = !diary[h.idx].done;
                    saveToFile();
                    break;
                case H_CARD:  g_selected = h.idx; break;
                default:      g_selected = -1;    break;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = (int)(short)HIWORD(wParam);
            g_scroll -= delta / 2;
            clampScroll();
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            g_hover = hitTest(pt.x, pt.y);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        case WM_CHAR: {
            wchar_t c = (wchar_t)wParam;
            if (c >= 32 && c != 127 && g_inputLen < MAX_DLINA - 1) {
                g_input[g_inputLen++] = c;
                g_input[g_inputLen] = 0;
                g_caretOn = true;
                invalidateInput(hWnd);
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_BACK) {
                if (g_inputLen > 0) {
                    g_inputLen--;
                    if (g_inputLen > 0 &&
                        g_input[g_inputLen] >= 0xDC00 && g_input[g_inputLen] <= 0xDFFF &&
                        g_input[g_inputLen - 1] >= 0xD800 && g_input[g_inputLen - 1] <= 0xDBFF)
                        g_inputLen--;
                    g_input[g_inputLen] = 0;
                }
                g_caretOn = true;
                invalidateInput(hWnd);
                return 0;
            }
            if (wParam == VK_RETURN) {
                addZapis(hWnd);
                return 0;
            }
            if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                pasteClipboard(hWnd);
                g_caretOn = true;
                invalidateInput(hWnd);
                return 0;
            }
            break;
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
            if (wParam == ID_TIMER_CARET) {
                g_caretOn = !g_caretOn;
                if (IsWindowVisible(hWnd)) invalidateInput(hWnd);
                return 0;
            }
            wchar_t today[11];
            getCurrentDate(today);
            for (int i = 0; i < count; i++) {
                if (wcscmp(diary[i].date, today) == 0 &&
                    diary[i].priority == 3 && !diary[i].done) {
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
            KillTimer(hWnd, ID_TIMER_CARET);
            saveToFile();
            Shell_NotifyIconW(NIM_DELETE, &nid);
            UnregisterHotKey(hWnd, ID_HOTKEY);
            if (gF.title)  DeleteObject(gF.title);
            if (gF.geo16)  DeleteObject(gF.geo16);
            if (gF.seg9)   DeleteObject(gF.seg9);
            if (gF.seg10)  DeleteObject(gF.seg10);
            if (gF.seg11)  DeleteObject(gF.seg11);
            if (gF.seg11i) DeleteObject(gF.seg11i);
            if (gF.seg11s) DeleteObject(gF.seg11s);
            if (gF.btn)    DeleteObject(gF.btn);
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
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = hIconColor;
    RegisterClassW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT wr = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRect(&wr, style, FALSE);

    hMainWnd = CreateWindowExW(
        0, L"DiaryMainWnd", L"Ежедневник",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL);

    if (!hMainWnd) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    applyMainWindowChrome(hMainWnd);

    createPopupWindow();

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hMainWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIconColor;
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
