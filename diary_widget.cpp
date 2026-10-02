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
#define CLR_BG          RGB(31, 24, 21)      // тёмный шоколад #1F1815
#define CLR_SURFACE     RGB(42, 31, 26)      // чуть светлее #2A1F1A
#define CLR_SURFACE_HOV RGB(58, 44, 36)      // hover #3A2C24
#define CLR_INPUT       RGB(26, 19, 16)      // темнее фона #1A1310
#define CLR_TEXT        RGB(232, 220, 200)   // cream #E8DCC8
#define CLR_MUTED       RGB(168, 152, 128)   // приглушённый беж #A89880
#define CLR_DIVIDER     RGB(58, 44, 34)      // еле видимый #3A2C22
#define CLR_ACCENT      RGB(184, 149, 106)   // тёплое золото #B8956A
#define CLR_ACCENT2     RGB(168, 152, 128)   // muted #A89880
#define CLR_ACCENT3     RGB(122, 100, 72)    // тусклая бронза #7A6448

#define WIN_W             560
#define WIN_H             800
#define UI_PAD            28
#define UI_HEADER_H       255
#define UI_BODY_TOP       UI_HEADER_H
#define UI_RING_D         88
#define UI_RING_CY        62
#define UI_STREAK_Y       114
#define UI_STREAK_H       26
#define UI_STRIP_Y        148
#define UI_DAY_PILL_Y     182
#define UI_DAY_PILL_H     28
#define UI_TABS_Y         220
#define UI_TABS_H         32
#define UI_STRIP_H        28
#define UI_INPUT_H        52
#define UI_BTN_H          42
#define UI_BTN_GAP        10
#define UI_CARD_H         52
#define UI_CARD_GAP       8
#define UI_ROW            (UI_CARD_H + UI_CARD_GAP)
#define HAB_ROW_H         40
#define MAX_HABITS        50
#define MAX_PRACTICES     2000
#define MAX_LANGUAGES     10
#define LANG_NAME_MAX     32
#define LANG_GOAL_MAX     64
#define MAX_WORDS         500
#define WORD_ORIG_MAX     64
#define WORD_TRANS_MAX    64
#define PRACT_NAME_MAX    32
#define HAB_NAME_MAX      64
#define ID_TIMER_CARET    2
#define STREAK_REQUIRE_ALL 1

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

// ===== Playfair Display =====
static const wchar_t *g_displayFace = L"Georgia";
static int g_displayWeight = FW_NORMAL;

static const wchar_t *kFontFiles[12] = {
    L"Inter_28pt-Black.ttf",         L"Inter_28pt-Bold.ttf",
    L"Inter_24pt-Black.ttf",         L"Inter_24pt-Bold.ttf",
    L"Inter_18pt-Black.ttf",         L"Inter_18pt-Bold.ttf",
    L"Inter_18pt-SemiBold.ttf",      L"Inter_18pt-Medium.ttf",
    L"Inter_18pt-Regular.ttf",       L"Inter_18pt-Light.ttf",
    L"Inter_24pt-SemiBold.ttf",      L"Inter_24pt-Regular.ttf"
};
static wchar_t g_fontLoaded[12][MAX_PATH];

static bool faceAvailable(const wchar_t *face, int weight) {
    HDC dc = GetDC(NULL);
    HFONT f = CreateFontW(-40, 0, 0, 0, weight, TRUE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);
    HGDIOBJ old = SelectObject(dc, f);
    wchar_t actual[LF_FACESIZE] = {0};
    GetTextFaceW(dc, LF_FACESIZE, actual);
    SelectObject(dc, old);
    DeleteObject(f);
    ReleaseDC(NULL, dc);
    return _wcsicmp(actual, face) == 0;
}

static void loadAppFonts() {
    wchar_t dir[MAX_PATH];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *(slash + 1) = 0;

    for (int i = 0; i < 12; i++) {
        g_fontLoaded[i][0] = 0;
        wchar_t path[MAX_PATH];
        swprintf(path, MAX_PATH, L"%ls%ls", dir, kFontFiles[i]);
        if (AddFontResourceExW(path, FR_PRIVATE, 0) > 0) {
            wcscpy(g_fontLoaded[i], path);
        } else if (AddFontResourceExW(kFontFiles[i], FR_PRIVATE, 0) > 0) {
            wcscpy(g_fontLoaded[i], kFontFiles[i]);
        }
    }

    if (faceAvailable(L"Inter", FW_BLACK)) {
        g_displayFace = L"Inter"; g_displayWeight = FW_BLACK;
    } else if (faceAvailable(L"Inter", FW_HEAVY)) {
        g_displayFace = L"Inter"; g_displayWeight = FW_HEAVY;
    } else if (faceAvailable(L"Inter", FW_BOLD)) {
        g_displayFace = L"Inter"; g_displayWeight = FW_BOLD;
    } else {
        g_displayFace = L"Segoe UI"; g_displayWeight = FW_BOLD;
        OutputDebugStringW(L"[planner] Inter not found, using Segoe UI\n");
    }
}

static void unloadAppFonts() {
    for (int i = 0; i < 12; i++)
        if (g_fontLoaded[i][0])
            RemoveFontResourceExW(g_fontLoaded[i], FR_PRIVATE, 0);
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

// ===== Состояние и данные главного окна =====
enum { H_NONE = 0, H_ADD, H_DEL, H_HIDE, H_INPUT, H_PRIO, H_CHECK, H_CARD,
       H_HAB_CHECK, H_HAB_DEL, H_HAB_ROW, H_HAB_ADDBTN, H_HAB_INPUT, H_HAB_OK,
       H_TAB, H_TODAY_ADD_TASK, H_TODAY_ADD_PRACTICE, H_TODAY_ADD_WORD, H_CALENDAR_ICON,
       H_PRACTICE_ADD, H_PRACTICE_PILL, H_PRACTICE_DEL,
       H_PF_TYPE, H_PF_MIN, H_PF_OK, H_PF_CANCEL, H_PF_PILL,
       H_LF_ROW, H_LF_OK, H_LF_CANCEL, H_LANG_ADD, H_LANG_DEL, H_LANG_OPEN, H_LANG_BACK,
       H_DAY_PILL, H_DAY_PREV, H_DAY_NEXT };
struct Hit { int kind; int idx; int idx2; };
enum { FOCUS_TASK = 0, FOCUS_HABIT };
enum { TAB_TODAY = 0, TAB_HABITS, TAB_TASKS, TAB_LANGUAGE, TAB_PRACTICE, TAB_COUNT };
static const wchar_t *TAB_NAMES[TAB_COUNT] = {
    L"TODAY", L"HABITS", L"TASKS", L"LANGUAGE", L"PRACTICE"
};
static int g_activeTab = TAB_TODAY;

static Hit  g_hover    = { H_NONE, -1, -1 };
static wchar_t g_input[MAX_DLINA] = L"";
static int  g_inputLen = 0;
static int  g_newPrio  = 2;
static int  g_scroll   = 0;
static int  g_selected = -1;
static bool g_caretOn  = true;
static bool g_tracking = false;
static int  g_focus    = FOCUS_TASK;
static int  g_viewH    = WIN_H;

static bool g_habAdding = false;
static wchar_t g_hInput[HAB_NAME_MAX] = L"";
static int  g_hInputLen = 0;

static int  g_streak  = 0;
static int  g_statDay = 0;

static struct {
    HFONT disp36, disp20, disp16;
    HFONT seg8, seg9, seg10, seg11, seg11i, seg11s, btn;
} gF = {0};

struct Habit {
    int id;
    wchar_t name[HAB_NAME_MAX];
    bool done[7];
};
struct HabitFileHeader { int version; int weekKey; int count; };

struct Practice {
    int id;
    wchar_t date[11];          // ДД.ММ.ГГГГ
    wchar_t type[PRACT_NAME_MAX]; // Yoga, Gym, Tennis...
    int minutes;
};
struct PracticeFileHeader { int version; int count; };

static Practice practices[MAX_PRACTICES];
static int practiceCount = 0;
static int g_practiceGoal = 30;  // цель: 30 мин/день

// ===== Language =====
struct Language {
    int id;
    wchar_t name[LANG_NAME_MAX];   // "АНГЛИЙСКИЙ"
    wchar_t goal[LANG_GOAL_MAX];   // "B1 → C1"
    int progress;                   // 0..100
    int wordsTotal;
    int streakDays;
};
struct LanguageFileHeader { int version; int count; };

static Language languages[MAX_LANGUAGES];
static int languageCount = 0;
static const wchar_t *LANGUAGES_FILE = L"languages.dat";

// ===== Words =====
struct Word {
    int id;
    int languageId;             // к какому языку (id)
    wchar_t original[WORD_ORIG_MAX];
    wchar_t transcription[WORD_TRANS_MAX];
    wchar_t translation[WORD_TRANS_MAX];
};
struct WordFileHeader { int version; int count; };

static Word words[MAX_WORDS];
static int wordCount = 0;
static const wchar_t *WORDS_FILE = L"words.dat";

// ===== Шаблоны слов (добавляются при добавлении языка) =====
struct WordTemplate {
    const wchar_t *orig;
    const wchar_t *trans;
    const wchar_t *mean;
};

// Английский — 20 слов
static const WordTemplate WORDS_EN[] = {
    { L"hello",       L"/хэлло́у/",       L"привет" },
    { L"thrive",      L"/срайв/",         L"процветать" },
    { L"focus",       L"/фо́кус/",         L"фокус, сосредоточиться" },
    { L"plan",        L"/плэн/",          L"план" },
    { L"dream",       L"/дрим/",          L"мечта" },
    { L"life",        L"/лайф/",          L"жизнь" },
    { L"love",        L"/лав/",           L"любовь" },
    { L"work",        L"/уорк/",          L"работа, работать" },
    { L"home",        L"/хоум/",          L"дом" },
    { L"friend",      L"/френд/",         L"друг" },
    { L"time",        L"/тайм/",          L"время" },
    { L"day",         L"/дэй/",           L"день" },
    { L"night",       L"/найт/",          L"ночь" },
    { L"morning",     L"/мо́рнинг/",       L"утро" },
    { L"water",       L"/уо́тер/",         L"вода" },
    { L"food",        L"/фуд/",           L"еда" },
    { L"book",        L"/бук/",           L"книга" },
    { L"music",       L"/мью́зик/",        L"музыка" },
    { L"smile",       L"/смайл/",         L"улыбка" },
    { L"soul",        L"/соул/",          L"душа" },
};
#define WORDS_EN_COUNT 20

// Армянский — 10 слов
static const WordTemplate WORDS_HY[] = {
    { L"Բարև",              L"/баре́в/",              L"привет" },
    { L"շնորհակալություն",  L"/шноракалутю́н/",       L"спасибо" },
    { L"Այո",               L"/айо́/",                L"да" },
    { L"Ոչ",                L"/воч/",                L"нет" },
    { L"Խնդրում եմ",        L"/хндру́м ем/",          L"пожалуйста" },
    { L"Բարի լույս",        L"/бари́ луйс/",          L"доброе утро" },
    { L"Բարի գիշեր",        L"/бари́ гише́р/",         L"доброй ночи" },
    { L"Ինչպես ես",         L"/инчпе́с эс/",          L"как дела" },
    { L"Սեր",               L"/сер/",                L"любовь" },
    { L"Կյանք",             L"/кянк/",               L"жизнь" },
};
#define WORDS_HY_COUNT 10

// Грузинский — 10 слов
static const WordTemplate WORDS_KA[] = {
    { L"გამარჯობა",       L"/гамарджо́ба/",     L"здравствуйте" },
    { L"მადლობა",         L"/мадло́ба/",         L"спасибо" },
    { L"დიახ",            L"/диа́х/",            L"да" },
    { L"არა",             L"/а́ра/",             L"нет" },
    { L"გთხოვ",           L"/гтхо́в/",           L"пожалуйста" },
    { L"დილა მშვიდობისა", L"/ди́ла мшвидо́биса/", L"доброе утро" },
    { L"ღამე მშვიდობისა", L"/га́ме мшвидо́биса/", L"доброй ночи" },
    { L"როგორ ხარ",       L"/ро́гор хар/",       L"как дела" },
    { L"სიყვარული",       L"/сикварю́ли/",       L"любовь" },
    { L"ცხოვრება",        L"/цховре́ба/",        L"жизнь" },
};
#define WORDS_KA_COUNT 10

// Китайский — 10 слов
static const WordTemplate WORDS_ZH[] = {
    { L"你好",            L"nǐ hǎo / ниха́о/",     L"привет" },
    { L"谢谢",            L"xiè xie / сесе́/",     L"спасибо" },
    { L"是",              L"shì / шы/",           L"да, быть" },
    { L"不",              L"bù / бу/",            L"нет, не" },
    { L"请",              L"qǐng / цин/",         L"пожалуйста" },
    { L"早上好",          L"zǎo shang hǎo / цзаоша́нха́о/", L"доброе утро" },
    { L"晚安",            L"wǎn ān / вана́нь/",    L"спокойной ночи" },
    { L"你好吗",          L"nǐ hǎo ma / ниха́о ма/", L"как дела" },
    { L"爱",              L"ài / ай/",            L"любовь" },
    { L"生活",            L"shēng huó / шэнхуо́/", L"жизнь" },
};
#define WORDS_ZH_COUNT 10

struct LanguageTemplate {
    const wchar_t *name;
    const wchar_t *goal;
    int progress;
    int wordsTotal;
    int streakDays;
};

static const LanguageTemplate LANG_TEMPLATES[] = {
    { L"АНГЛИЙСКИЙ", L"B1 \u2192 C1",      43, 1240, 23 },
    { L"АРМЯНСКИЙ",  L"алфавит + 500 слов", 12,   60,  5 },
    { L"ГРУЗИНСКИЙ", L"разговорный A2",     35,  380,  0 },
    { L"КИТАЙСКИЙ",  L"HSK 3 \u2192 HSK 5", 18,  210,  0 },
};
#define LANG_TEMPLATES_COUNT 4

static bool g_langForm = false;
static bool g_langSelected[LANG_TEMPLATES_COUNT] = { false, false, false, false };
static int g_openLang = -1;  // индекс открытого языка (-1 = список)

// Просматриваемый день (-1 = сегодня)
static int g_viewDay = -1;

// Forward declarations для функций работы с датами
static int todayDayNumber();
static int daysFromCivil(int y, int m, int d);

// Получить номер дня для просмотра
static int viewDayNum() {
    if (g_viewDay < 0) return todayDayNumber();
    return g_viewDay;
}

// Дата просмотра в формате ДД.ММ.ГГГГ
static void getViewDate(wchar_t *buf) {
    time_t t0 = time(NULL);
    struct tm *ti = localtime(&t0);
    int today = daysFromCivil(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
    int target = (g_viewDay < 0) ? today : g_viewDay;
    int diff = target - today;

    // Прибавить diff дней к текущей дате
    t0 += (time_t)diff * 86400;
    ti = localtime(&t0);
    swprintf(buf, 11, L"%02d.%02d.%04d",
             ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
}

// Проверка — просматриваем сегодня?
static bool isViewingToday() {
    return (g_viewDay < 0) || (g_viewDay == todayDayNumber());
}

// Заголовок для шапки: "ПЯТНИЦА, 2 ОКТЯБРЯ" или "СЕГОДНЯ, 2 ОКТЯБРЯ"
static void getViewHeaderDate(wchar_t *buf, int n) {
    static const wchar_t *wd[] = { L"ВОСКРЕСЕНЬЕ", L"ПОНЕДЕЛЬНИК", L"ВТОРНИК",
        L"СРЕДА", L"ЧЕТВЕРГ", L"ПЯТНИЦА", L"СУББОТА" };
    static const wchar_t *mo[] = { L"января", L"февраля", L"марта", L"апреля",
        L"мая", L"июня", L"июля", L"августа", L"сентября", L"октября",
        L"ноября", L"декабря" };

    time_t t0 = time(NULL);
    struct tm *ti = localtime(&t0);
    int today = daysFromCivil(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
    int target = (g_viewDay < 0) ? today : g_viewDay;
    int diff = target - today;

    t0 += (time_t)diff * 86400;
    ti = localtime(&t0);

    if (diff == 0)
        swprintf(buf, n, L"СЕГОДНЯ, %d %ls", ti->tm_mday, mo[ti->tm_mon]);
    else if (diff == -1)
        swprintf(buf, n, L"ВЧЕРА, %d %ls", ti->tm_mday, mo[ti->tm_mon]);
    else if (diff == 1)
        swprintf(buf, n, L"ЗАВТРА, %d %ls", ti->tm_mday, mo[ti->tm_mon]);
    else
        swprintf(buf, n, L"%ls, %d %ls", wd[ti->tm_wday], ti->tm_mday, mo[ti->tm_mon]);
}

// Состояние формы добавления практики
static bool g_practiceForm = false;
static wchar_t g_practiceTypeInput[PRACT_NAME_MAX] = L"";
static int g_practiceTypeLen = 0;
static wchar_t g_practiceMinInput[8] = L"";
static int g_practiceMinLen = 0;
static int g_practiceField = 0;  // 0 = тип, 1 = минуты
static const wchar_t *PRACTICES_FILE = L"practices.dat";

// Типы практик по умолчанию
static const wchar_t *PRACTICE_TYPES[] = {
    L"Yoga", L"Stretching", L"Gym", L"Boxing", L"Tennis", L"Run", L"Другое"
};
#define PRACTICE_TYPES_COUNT 7

static Habit habits[MAX_HABITS];
static int habitCount = 0;
static int g_habitWeekKey = 0;
static const wchar_t *HABITS_FILE = L"habits.dat";

static int daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static int todayDayNumber() {
    time_t t = time(NULL);
    struct tm *ti = localtime(&t);
    return daysFromCivil(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
}
static int todayWeekIdx() {
    time_t t = time(NULL);
    struct tm *ti = localtime(&t);
    return (ti->tm_wday + 6) % 7;
}
static int currentWeekKey() { return todayDayNumber() - todayWeekIdx(); }

static int parseDayNumber(const wchar_t *s) {
    int d = 0, m = 0, y = 0;
    if (swscanf(s, L"%d.%d.%d", &d, &m, &y) != 3) return -1;
    if (m < 1 || m > 12 || d < 1 || d > 31) return -1;
    return daysFromCivil(y, m, d);
}

static void getTodayStats(int *done, int *total) {
    wchar_t d[11];
    getViewDate(d);
    *done = 0; *total = 0;
    for (int i = 0; i < count; i++) {
        if (wcscmp(diary[i].date, d) == 0) {
            (*total)++;
            if (diary[i].done) (*done)++;
        }
    }
}

static bool dayComplete(const int *dn, int day) {
    // Проверка 1: все задачи выполнены?
    int total = 0, done = 0;
    for (int i = 0; i < count; i++)
        if (dn[i] == day) { total++; if (diary[i].done) done++; }
    bool tasksOk = (total > 0) && (STREAK_REQUIRE_ALL ? (done == total) : (done > 0));

    // Проверка 2: была ли практика в этот день?
    bool practiceOk = false;
    for (int i = 0; i < practiceCount; i++) {
        if (parseDayNumber(practices[i].date) == day) {
            practiceOk = true;
            break;
        }
    }

    // День засчитан: либо все задачи, либо есть практика
    return tasksOk || practiceOk;
}

static int computeStreak() {
    int dn[MAX_ZAPISEY];
    for (int i = 0; i < count; i++) dn[i] = parseDayNumber(diary[i].date);
    int d = todayDayNumber();
    if (!dayComplete(dn, d)) d--;
    int s = 0;
    while (s <= count && dayComplete(dn, d)) { s++; d--; }
    return s;
}
static void updateStreak() { g_streak = computeStreak(); g_statDay = todayDayNumber(); }

static const wchar_t *dayWord(int n) {
    int m100 = n % 100, m10 = n % 10;
    if (m100 >= 11 && m100 <= 14) return L"дней";
    if (m10 == 1) return L"день";
    if (m10 >= 2 && m10 <= 4) return L"дня";
    return L"дней";
}

static void saveHabits() {
    FILE *f = _wfopen(HABITS_FILE, L"wb");
    if (!f) return;
    HabitFileHeader h = { 1, g_habitWeekKey, habitCount };
    fwrite(&h, sizeof(h), 1, f);
    fwrite(habits, sizeof(Habit), habitCount, f);
    fclose(f);
}

static void rolloverHabits() {
    int wk = currentWeekKey();
    if (wk == g_habitWeekKey) return;
    for (int i = 0; i < habitCount; i++)
        for (int k = 0; k < 7; k++) habits[i].done[k] = false;
    g_habitWeekKey = wk;
    saveHabits();
}

static void loadHabits() {
    habitCount = 0;
    g_habitWeekKey = currentWeekKey();
    FILE *f = _wfopen(HABITS_FILE, L"rb");
    if (!f) return;
    HabitFileHeader h;
    if (fread(&h, sizeof(h), 1, f) == 1 && h.version == 1 && h.count >= 0) {
        if (h.count > MAX_HABITS) h.count = MAX_HABITS;
        habitCount = (int)fread(habits, sizeof(Habit), h.count, f);
        g_habitWeekKey = h.weekKey;
        for (int i = 0; i < habitCount; i++) {
            habits[i].name[HAB_NAME_MAX - 1] = 0;
            for (int k = 0; k < 7; k++)
                habits[i].done[k] = (*(unsigned char*)&habits[i].done[k] != 0);
        }
    }
    fclose(f);
    rolloverHabits();
}

// ===== Practice: сохранение / загрузка =====
static void savePractices() {
    FILE *f = _wfopen(PRACTICES_FILE, L"wb");
    if (!f) return;
    PracticeFileHeader h = { 1, practiceCount };
    fwrite(&h, sizeof(h), 1, f);
    fwrite(practices, sizeof(Practice), practiceCount, f);
    fclose(f);
}

static void loadPractices() {
    practiceCount = 0;
    FILE *f = _wfopen(PRACTICES_FILE, L"rb");
    if (!f) return;
    PracticeFileHeader h;
    if (fread(&h, sizeof(h), 1, f) == 1 && h.version == 1 && h.count >= 0) {
        if (h.count > MAX_PRACTICES) h.count = MAX_PRACTICES;
        practiceCount = (int)fread(practices, sizeof(Practice), h.count, f);
    }
    fclose(f);
}

// Минут практики за сегодня
static int practiceMinutesToday() {
    wchar_t d[11];
    getViewDate(d);
    int sum = 0;
    for (int i = 0; i < practiceCount; i++)
        if (wcscmp(practices[i].date, d) == 0)
            sum += practices[i].minutes;
    return sum;
}

// Streak практики (дни подряд)
static int practiceStreak() {
    int dn[MAX_PRACTICES];
    for (int i = 0; i < practiceCount; i++) dn[i] = parseDayNumber(practices[i].date);
    int d = todayDayNumber();
    // если сегодня ещё не было, начинаем со вчера
    bool todayDone = false;
    for (int i = 0; i < practiceCount; i++) if (dn[i] == d) { todayDone = true; break; }
    if (!todayDone) d--;
    int s = 0;
    while (s < 365) {
        bool done = false;
        for (int i = 0; i < practiceCount; i++) if (dn[i] == d) { done = true; break; }
        if (!done) break;
        s++; d--;
    }
    return s;
}

// ===== Language: сохранение / загрузка =====
static void saveLanguages() {
    FILE *f = _wfopen(LANGUAGES_FILE, L"wb");
    if (!f) return;
    LanguageFileHeader h = { 1, languageCount };
    fwrite(&h, sizeof(h), 1, f);
    fwrite(languages, sizeof(Language), languageCount, f);
    fclose(f);
}

static void loadLanguages() {
    languageCount = 0;
    FILE *f = _wfopen(LANGUAGES_FILE, L"rb");
    if (!f) return;
    LanguageFileHeader h;
    if (fread(&h, sizeof(h), 1, f) == 1 && h.version == 1 && h.count >= 0) {
        if (h.count > MAX_LANGUAGES) h.count = MAX_LANGUAGES;
        languageCount = (int)fread(languages, sizeof(Language), h.count, f);
        for (int i = 0; i < languageCount; i++) {
            languages[i].name[LANG_NAME_MAX - 1] = 0;
            languages[i].goal[LANG_GOAL_MAX - 1] = 0;
        }
    }
    fclose(f);
}

// ===== Words: сохранение / загрузка =====
static void saveWords() {
    FILE *f = _wfopen(WORDS_FILE, L"wb");
    if (!f) return;
    WordFileHeader h = { 1, wordCount };
    fwrite(&h, sizeof(h), 1, f);
    fwrite(words, sizeof(Word), wordCount, f);
    fclose(f);
}

static void loadWords() {
    wordCount = 0;
    FILE *f = _wfopen(WORDS_FILE, L"rb");
    if (!f) return;
    WordFileHeader h;
    if (fread(&h, sizeof(h), 1, f) == 1 && h.version == 1 && h.count >= 0) {
        if (h.count > MAX_WORDS) h.count = MAX_WORDS;
        wordCount = (int)fread(words, sizeof(Word), h.count, f);
        for (int i = 0; i < wordCount; i++) {
            words[i].original[WORD_ORIG_MAX - 1] = 0;
            words[i].transcription[WORD_TRANS_MAX - 1] = 0;
            words[i].translation[WORD_TRANS_MAX - 1] = 0;
        }
    }
    fclose(f);
}

// Добавить слова для конкретного языка
static void addWordsForLanguage(int langId, const wchar_t *langName) {
    // Уже есть слова для этого языка?
    bool exists = false;
    for (int i = 0; i < wordCount; i++) {
        if (words[i].languageId == langId) { exists = true; break; }
    }
    if (exists) return;

    const WordTemplate *tmpl = NULL;
    int tmplCount = 0;

    if (wcscmp(langName, L"АНГЛИЙСКИЙ") == 0) {
        tmpl = WORDS_EN; tmplCount = WORDS_EN_COUNT;
    } else if (wcscmp(langName, L"АРМЯНСКИЙ") == 0) {
        tmpl = WORDS_HY; tmplCount = WORDS_HY_COUNT;
    } else if (wcscmp(langName, L"ГРУЗИНСКИЙ") == 0) {
        tmpl = WORDS_KA; tmplCount = WORDS_KA_COUNT;
    } else if (wcscmp(langName, L"КИТАЙСКИЙ") == 0) {
        tmpl = WORDS_ZH; tmplCount = WORDS_ZH_COUNT;
    }

    if (!tmpl) return;

    for (int i = 0; i < tmplCount && wordCount < MAX_WORDS; i++) {
        Word *w = &words[wordCount];
        w->id = wordCount + 1;
        w->languageId = langId;
        wcsncpy(w->original, tmpl[i].orig, WORD_ORIG_MAX - 1);
        w->original[WORD_ORIG_MAX - 1] = 0;
        wcsncpy(w->transcription, tmpl[i].trans, WORD_TRANS_MAX - 1);
        w->transcription[WORD_TRANS_MAX - 1] = 0;
        wcsncpy(w->translation, tmpl[i].mean, WORD_TRANS_MAX - 1);
        w->translation[WORD_TRANS_MAX - 1] = 0;
        wordCount++;
    }
    saveWords();
}




struct Layout {
    int habTitle, habRows, habAdd, taskTitle, inputY, btnY, listY, contentH;
};

static Layout getLayout() {
    Layout L;
    int y = 16;
    L.habTitle = y;  y += 36;
    L.habRows  = y;  y += (habitCount > 0 ? habitCount : 1) * HAB_ROW_H;
    y += 10;
    L.habAdd   = y;  y += (g_habAdding ? 36 : 32);
    y += 28;
    L.taskTitle = y; y += 36;
    L.inputY   = y;  y += UI_INPUT_H + 12;
    L.btnY     = y;  y += UI_BTN_H + 20;
    L.listY    = y;
    int listH  = count > 0 ? count * UI_ROW - UI_CARD_GAP : 250;
    L.contentH = y + listH + 24;

    // Для LANGUAGE — переопределяем contentH
    if (g_activeTab == TAB_LANGUAGE) {
        if (g_openLang >= 0 && g_openLang < languageCount) {
            // Сводка языка: header + название + цель + прогресс + статистика + слова
            int langId = languages[g_openLang].id;
            int wc = 0;
            for (int i = 0; i < wordCount; i++)
                if (words[i].languageId == langId) wc++;
            int h = 50 + 70 + 50 + 22 + 30 + 50 + 40;
            h += wc * 56;
            h += 60;
            L.contentH = h;
        } else {
            // Список языков
            int h = 56 + (languageCount > 0 ? languageCount * 116 : 130) + 60;
            L.contentH = h;
        }
    }

    // Для PRACTICE — тоже считаем
    if (g_activeTab == TAB_PRACTICE) {
        if (g_practiceForm) {
            L.contentH = 450;
        } else {
            // ПРАКТИКА: заголовок + сводка + типы + кнопка + список + календарь
            int h = 56 + 42 + 40 + 30 + 80 + 44 + 28 + 5 * 30 + 28 + 7 * 20 + 60;
            L.contentH = h;
        }
    }

    return L;
}

static int viewBodyH() { return g_viewH - UI_BODY_TOP; }
static int sy(int contentY) { return UI_BODY_TOP + contentY - g_scroll; }

static int maxScroll() {
    int m = getLayout().contentH - viewBodyH();
    return m > 0 ? m : 0;
}
static void clampScroll() {
    int m = maxScroll();
    if (g_scroll > m) g_scroll = m;
    if (g_scroll < 0) g_scroll = 0;
}
static void ensureVisible(int contentY, int h) {
    int vh = viewBodyH();
    if (contentY + h > g_scroll + vh) g_scroll = contentY + h - vh;
    if (contentY < g_scroll) g_scroll = contentY;
    clampScroll();
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
    getViewDate(z->date);
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
            g_fTitle = makeFont(g_displayFace, 26, FALSE, g_displayWeight);
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
            RECT rcTitle = {PAD, 8, rc.right - PAD, 42};
            DrawTextW(memDC, L"FOCUS BITCH.", -1, &rcTitle,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(memDC, g_fDate);
            SetTextColor(memDC, CLR_MUTED);
            RECT rcSub = {PAD, 42, rc.right - PAD, 56};
            DrawTextW(memDC, L"follow the plan, not the mood.", -1, &rcSub,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            fillRectColor(memDC, PAD, 58, rc.right - PAD, 59, CLR_DIVIDER);

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
    getViewHeaderDate(buf, n);
}
// ===== Мелкие рисовалки =====
static void gCheck(Gdiplus::Graphics &g, float cx, float cy, float s) {
    Gdiplus::Pen chk(Gdiplus::Color(255, 255, 255, 255), 2.0f * s);
    chk.SetStartCap(Gdiplus::LineCapRound);
    chk.SetEndCap(Gdiplus::LineCapRound);
    chk.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::PointF pts[3] = {
        Gdiplus::PointF(cx - 4.5f * s, cy + 0.5f * s),
        Gdiplus::PointF(cx - 1.0f * s, cy + 4.0f * s),
        Gdiplus::PointF(cx + 4.5f * s, cy - 3.5f * s) };
    g.DrawLines(&chk, pts, 3);
}

static void gCross(Gdiplus::Graphics &g, float cx, float cy, float half, COLORREF c) {
    Gdiplus::Pen p(gc(c), 1.6f);
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&p, cx - half, cy - half, cx + half, cy + half);
    g.DrawLine(&p, cx - half, cy + half, cx + half, cy - half);
}

static void gFlame(Gdiplus::Graphics &g, float fx, float fy) {
    using namespace Gdiplus;
    GraphicsPath p;
    p.AddBezier(fx + 7.0f, fy + 18.0f, fx - 1.0f, fy + 14.0f, fx + 1.0f, fy + 6.0f, fx + 7.0f, fy);
    p.AddBezier(fx + 7.0f, fy, fx + 8.0f, fy + 5.0f, fx + 14.0f, fy + 8.0f, fx + 14.0f, fy + 12.0f);
    p.AddBezier(fx + 14.0f, fy + 12.0f, fx + 14.0f, fy + 16.0f, fx + 11.0f, fy + 18.0f, fx + 7.0f, fy + 18.0f);
    p.CloseFigure();
    LinearGradientBrush br(PointF(fx, fy), PointF(fx, fy + 18.0f), gc(CLR_ACCENT2), gc(CLR_ACCENT));
    g.FillPath(&br, &p);
    SolidBrush hi(gc(CLR_TEXT, 170));
    g.FillEllipse(&hi, fx + 5.0f, fy + 11.0f, 4.0f, 5.5f);
}

static void gHLine(Gdiplus::Graphics &g, float x, float y, float w, COLORREF c) {
    Gdiplus::SolidBrush b(gc(c));
    g.FillRectangle(&b, x, y, w, 1.0f);
}

// ===== Хит-тест помощники =====
static bool ptIn(const RECT &r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}
static bool inCircle(int x, int y, float cx, float cy, float r) {
    float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

static RECT rcInput(const Layout &L) {
    int y = sy(L.inputY);
    RECT r = { UI_PAD, y, WIN_W - UI_PAD, y + UI_INPUT_H };
    return r;
}
static RECT rcBtn(const Layout &L, int i) {
    static const int w[3] = { 130, 110, 120 };
    int x = UI_PAD;
    for (int k = 0; k < i; k++) x += w[k] + UI_BTN_GAP;
    int y = sy(L.btnY);
    RECT r = { x, y, x + w[i], y + UI_BTN_H };
    return r;
}
static RECT rcHabAdd(const Layout &L)   { int y = sy(L.habAdd); RECT r = { UI_PAD, y, UI_PAD + 180, y + 32 }; return r; }
static RECT rcHabInput(const Layout &L) { int y = sy(L.habAdd); RECT r = { UI_PAD, y, 422, y + 36 }; return r; }
static RECT rcHabOk(const Layout &L)    { int y = sy(L.habAdd); RECT r = { 432, y, WIN_W - UI_PAD, y + 36 }; return r; }

static float prioCx(int k) { return (float)(WIN_W - UI_PAD - 20 - (2 - k) * 22); }
static float habCx(int k)  { return 488.0f - (6 - k) * 28.0f; }
#define HAB_DEL_CX 520.0f

// Forward declarations для табов
static RECT rcTab(int i);
static void paintTabs(HDC dc, Gdiplus::Graphics &g);
static Hit hitTestTabs(int x, int y);
// Forward declarations для PRACTICE
static int practiceMinutesToday();
static int practiceStreak();
static void saveLanguages();
static void loadLanguages();
static void saveWords();
static void loadWords();
static void addWordsForLanguage(int langId, const wchar_t *langName);
static void cancelLangForm();
static void commitLangForm();
static RECT rcLfRow(int i);
static RECT rcLfOkBtn();
static RECT rcLfCancelBtn();
// Forward declarations для формы практики
static RECT rcPfTypeBox();
static RECT rcPfMinBox();
static RECT rcPfOkBtn();
static RECT rcPfCancelBtn();
static RECT rcPfPill(int i);
static void cancelPracticeForm();
static void commitPracticeForm();
// Forward declarations для календаря и кнопок TODAY
static RECT rcCalendarIcon();
static RECT rcDayPill(int i);
static RECT rcDayPrev();
static RECT rcDayNext();
static RECT rcTodayAddTask(const Layout &L);
static RECT rcTodayAddPractice(const Layout &L);
static RECT rcTodayAddWord(const Layout &L);

static Hit hitTest(int x, int y) {
    Hit h = { H_NONE, -1, -1 };

    // === Форма языков (приоритет выше практики) ===
    if (g_langForm) {
        if (ptIn(rcLfOkBtn(), x, y))     { h.kind = H_LF_OK;     return h; }
        if (ptIn(rcLfCancelBtn(), x, y)) { h.kind = H_LF_CANCEL; return h; }
        for (int i = 0; i < LANG_TEMPLATES_COUNT; i++) {
            if (ptIn(rcLfRow(i), x, y)) {
                h.kind = H_LF_ROW; h.idx = i; return h;
            }
        }
        h.kind = H_NONE;
        return h;
    }

    // === Форма практики (модальный режим) ===
    if (g_practiceForm) {
        if (ptIn(rcPfTypeBox(), x, y)) { h.kind = H_PF_TYPE;   return h; }
        if (ptIn(rcPfMinBox(),  x, y)) { h.kind = H_PF_MIN;    return h; }
        if (ptIn(rcPfOkBtn(),   x, y)) { h.kind = H_PF_OK;     return h; }
        if (ptIn(rcPfCancelBtn(), x, y)) { h.kind = H_PF_CANCEL; return h; }
        for (int i = 0; i < PRACTICE_TYPES_COUNT; i++) {
            if (ptIn(rcPfPill(i), x, y)) {
                h.kind = H_PF_PILL; h.idx = i; return h;
            }
        }
        // Всё остальное клики — игнорируем, пока форма открыта
        h.kind = H_NONE;
        return h;
    }

    // Сначала проверяем табы
    Hit ht = hitTestTabs(x, y);
    if (ht.kind != H_NONE) return ht;

    // Иконка календаря в шапке
    RECT calIcon = rcCalendarIcon();
    if (ptIn(calIcon, x, y)) {
        h.kind = H_CALENDAR_ICON;
        return h;
    }

    if (ptIn(rcDayPrev(), x, y)) { h.kind = H_DAY_PREV; return h; }
    if (ptIn(rcDayNext(), x, y)) { h.kind = H_DAY_NEXT; return h; }
    for (int i = 0; i < 7; i++) {
        if (ptIn(rcDayPill(i), x, y)) {
            h.kind = H_DAY_PILL; h.idx = i; return h;
        }
    }

    if (y < UI_BODY_TOP) return h;
    Layout L = getLayout();

    // Кнопки быстрых действий (только на вкладке TODAY)
    if (g_activeTab == TAB_TODAY) {
        if (ptIn(rcTodayAddTask(L), x, y))     { h.kind = H_TODAY_ADD_TASK;     return h; }
        if (ptIn(rcTodayAddPractice(L), x, y)) { h.kind = H_TODAY_ADD_PRACTICE; return h; }
        if (ptIn(rcTodayAddWord(L), x, y))     { h.kind = H_TODAY_ADD_WORD;     return h; }
    }

    // Клики на вкладке LANGUAGE
    if (g_activeTab == TAB_LANGUAGE) {
        // === Если открыта сводка языка ===
        if (g_openLang >= 0 && g_openLang < languageCount) {
            RECT rBack = { UI_PAD, sy(16), UI_PAD + 100, sy(16) + 30 };
            if (ptIn(rBack, x, y)) {
                h.kind = H_LANG_BACK;
                return h;
            }
            return h;
        }

        // === Список языков ===
        int ya = sy(16) + 56;  // ya — позиция отрисовки, y — координата клика
        if (languageCount == 0) {
            ya += 130;
        } else {
            for (int i = 0; i < languageCount; i++) {
                int yRow = ya;
                // Зона × — справа сверху
                RECT rx = { WIN_W - UI_PAD - 32, yRow - 4, WIN_W - UI_PAD + 4, yRow + 30 };
                if (ptIn(rx, x, y)) {
                    h.kind = H_LANG_DEL;
                    h.idx = i;
                    return h;
                }
                // Зона клика на весь язык
                RECT rRow = { UI_PAD, yRow, WIN_W - UI_PAD, yRow + 116 };
                if (ptIn(rRow, x, y)) {
                    h.kind = H_LANG_OPEN;
                    h.idx = i;
                    return h;
                }
                ya += 116;
            }
        }
        // Кнопка "+ добавить язык"
        RECT rAdd = { UI_PAD, ya, WIN_W - UI_PAD, ya + 30 };
        if (ptIn(rAdd, x, y)) {
            h.kind = H_LANG_ADD;
            return h;
        }
    }

    // Клики на вкладке PRACTICE
    if (g_activeTab == TAB_PRACTICE) {
        int py0 = sy(16);

        // Пилюли типов практик
        int pillY = py0 + 56 + 42 + 40 + 30;
        int px = UI_PAD;
        int py = pillY;
        int pillH = 32;
        HDC dc = GetDC(NULL);
        SelectObject(dc, gF.seg10);
        for (int i = 0; i < PRACTICE_TYPES_COUNT; i++) {
            SIZE sz;
            GetTextExtentPoint32W(dc, PRACTICE_TYPES[i], (int)wcslen(PRACTICE_TYPES[i]), &sz);
            int pillW = sz.cx + 24;
            if (px + pillW > WIN_W - UI_PAD) {
                px = UI_PAD; py += pillH + 6;
            }
            RECT rp = { px, py, px + pillW, py + pillH };
            if (ptIn(rp, x, y)) {
                ReleaseDC(NULL, dc);
                h.kind = H_PRACTICE_PILL;
                h.idx = i;
                return h;
            }
            px += pillW + 6;
        }
        ReleaseDC(NULL, dc);

        // Кнопка "+ добавить практику"
        int btnY = py + pillH + 24;
        RECT rAdd = { UI_PAD, btnY, WIN_W - UI_PAD, btnY + 30 };
        if (ptIn(rAdd, x, y)) {
            h.kind = H_PRACTICE_ADD;
            return h;
        }

        // Записи за сегодня (× для удаления)
        int listY = btnY + 44 + 28;
        wchar_t today[11];
        getCurrentDate(today);
        int shown = 0;
        for (int i = practiceCount - 1; i >= 0 && shown < 5; i--) {
            if (wcscmp(practices[i].date, today) != 0) continue;
            int rowY = listY + shown * 30;
            RECT rx = { WIN_W - UI_PAD - 24, rowY, WIN_W - UI_PAD, rowY + 30 };
            if (ptIn(rx, x, y)) {
                h.kind = H_PRACTICE_DEL;
                h.idx = i;
                return h;
            }
            shown++;
        }
    }

    for (int i = 0; i < habitCount; i++) {
        int top = sy(L.habRows + i * HAB_ROW_H);
        if (y < top || y >= top + HAB_ROW_H || x < UI_PAD || x >= WIN_W - UI_PAD) continue;
        float cy = top + HAB_ROW_H / 2.0f;
        h.idx = i;
        if (inCircle(x, y, HAB_DEL_CX, cy, 10.0f)) { h.kind = H_HAB_DEL; return h; }
        for (int k = 0; k < 7; k++)
            if (inCircle(x, y, habCx(k), cy, 12.0f)) { h.kind = H_HAB_CHECK; h.idx2 = k; return h; }
        h.kind = H_HAB_ROW;
        return h;
    }
    if (g_habAdding) {
        RECT ri = rcHabInput(L), ro = rcHabOk(L);
        if (ptIn(ri, x, y)) { h.kind = H_HAB_INPUT; return h; }
        if (ptIn(ro, x, y)) { h.kind = H_HAB_OK;    return h; }
    } else {
        RECT ra = rcHabAdd(L);
        if (ptIn(ra, x, y)) { h.kind = H_HAB_ADDBTN; return h; }
    }

    for (int b = 0; b < 3; b++) {
        RECT r = rcBtn(L, b);
        if (ptIn(r, x, y)) { h.kind = H_ADD + b; return h; }
    }

    RECT ri = rcInput(L);
    if (ptIn(ri, x, y)) {
        float cy = ri.top + UI_INPUT_H / 2.0f;
        for (int k = 0; k < 3; k++)
            if (inCircle(x, y, prioCx(k), cy, 11.0f)) { h.kind = H_PRIO; h.idx = k + 1; return h; }
        h.kind = H_INPUT;
        return h;
    }

    int rel = (y - UI_BODY_TOP + g_scroll) - L.listY;
    if (rel >= 0 && x >= UI_PAD && x < WIN_W - UI_PAD) {
        int r = rel / UI_ROW, within = rel % UI_ROW;
        if (within < UI_CARD_H && r < count) {
            int idx = count - 1 - r;
            float ccy = (float)(sy(L.listY + r * UI_ROW) + UI_CARD_H / 2);
            h.idx = idx;
            h.kind = inCircle(x, y, UI_PAD + 26.0f, ccy, 14.0f) ? H_CHECK : H_CARD;
        }
    }
    return h;
}

static void updateHover(HWND hWnd, int x, int y) {
    Hit h = hitTest(x, y);
    if (h.kind != g_hover.kind || h.idx != g_hover.idx || h.idx2 != g_hover.idx2) {
        g_hover = h;
        InvalidateRect(hWnd, NULL, FALSE);
    }
}

static bool isHabHover(int i) {
    return g_hover.idx == i && (g_hover.kind == H_HAB_ROW ||
           g_hover.kind == H_HAB_CHECK || g_hover.kind == H_HAB_DEL);
}

struct TextBuf { wchar_t *s; int *len; int cap; };
static TextBuf activeBuf() {
    TextBuf b;
    if (g_focus == FOCUS_HABIT && g_habAdding) { b.s = g_hInput; b.len = &g_hInputLen; b.cap = HAB_NAME_MAX; }
    else                                       { b.s = g_input;  b.len = &g_inputLen;  b.cap = MAX_DLINA; }
    return b;
}

static void bufBackspace(wchar_t *s, int *len) {
    if (*len <= 0) return;
    (*len)--;
    if (*len > 0 && s[*len] >= 0xDC00 && s[*len] <= 0xDFFF &&
        s[*len - 1] >= 0xD800 && s[*len - 1] <= 0xDBFF)
        (*len)--;
    s[*len] = 0;
}

static void pasteClipboard(HWND hWnd, wchar_t *s, int *len, int cap) {
    if (!OpenClipboard(hWnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t *p = (const wchar_t*)GlobalLock(h);
        if (p) {
            for (; *p && *len < cap - 1; p++)
                if (*p >= 32) s[(*len)++] = *p;
            s[*len] = 0;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}

static void invalidateActiveInput(HWND hWnd) {
    Layout L = getLayout();
    RECT r = (g_focus == FOCUS_HABIT && g_habAdding) ? rcHabInput(L) : rcInput(L);
    InvalidateRect(hWnd, &r, FALSE);
}

// ===== Действия =====
static void toggleDone(int idx) {
    diary[idx].done = !diary[idx].done;
    saveToFile();
    updateStreak();
}

static void cancelHabitAdd() {
    g_habAdding = false;
    g_hInputLen = 0; g_hInput[0] = 0;
    g_focus = FOCUS_TASK;
    clampScroll();
}

static void commitHabit(HWND hWnd) {
    if (g_hInputLen == 0) { cancelHabitAdd(); InvalidateRect(hWnd, NULL, FALSE); return; }
    if (habitCount >= MAX_HABITS) {
        MessageBox(hWnd, L"Достигнут лимит привычек (50).", L"Внимание", MB_OK | MB_ICONINFORMATION);
        return;
    }
    Habit *h = &habits[habitCount];
    memset(h, 0, sizeof(Habit));
    h->id = (habitCount == 0) ? 1 : habits[habitCount - 1].id + 1;
    wcscpy(h->name, g_hInput);
    habitCount++;
    saveHabits();
    cancelHabitAdd();
    InvalidateRect(hWnd, NULL, FALSE);
}

static void deleteHabit(HWND hWnd, int idx) {
    if (idx < 0 || idx >= habitCount) return;
    if (MessageBox(hWnd, L"Удалить привычку?", L"Подтверждение",
                   MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    for (int i = idx; i < habitCount - 1; i++) habits[i] = habits[i + 1];
    habitCount--;
    saveHabits();
    clampScroll();
    InvalidateRect(hWnd, NULL, FALSE);
}
// ===== Кнопки =====
static void paintAccentButton(HDC dc, Gdiplus::Graphics &g, const RECT &r,
                              const wchar_t *label, bool hov, bool plus) {
    int w = r.right - r.left, h = r.bottom - r.top;
    gFillRound(g, (float)r.left, (float)r.top, (float)w, (float)h, 12,
               hov ? mixColor(CLR_ACCENT, RGB(255, 255, 255), 14) : CLR_ACCENT);

    SelectObject(dc, gF.btn);
    SIZE sz;
    GetTextExtentPoint32W(dc, label, (int)wcslen(label), &sz);
    int group = plus ? 12 + 8 + sz.cx : sz.cx;
    int gx = r.left + (w - group) / 2;

    if (plus) {
        float cx = gx + 6.0f, cy = r.top + h / 2.0f;
        Gdiplus::Pen pen(gc(CLR_BG), 2.0f);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&pen, cx - 5.0f, cy, cx + 5.0f, cy);
        g.DrawLine(&pen, cx, cy - 5.0f, cx, cy + 5.0f);
    }
    g.Flush(Gdiplus::FlushIntentionSync);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, CLR_BG);
    RECT tr = r;
    if (plus) tr.left = gx + 20;
    DrawTextW(dc, label, -1, &tr, (plus ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE);
}

static void paintSurfaceButton(HDC dc, Gdiplus::Graphics &g, const RECT &r,
                               const wchar_t *label, bool hov, COLORREF fg) {
    gFillRound(g, (float)r.left, (float)r.top, (float)(r.right - r.left),
               (float)(r.bottom - r.top), 12, hov ? CLR_SURFACE_HOV : CLR_SURFACE);
    g.Flush(Gdiplus::FlushIntentionSync);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.btn);
    SetTextColor(dc, fg);
    RECT tr = r;
    DrawTextW(dc, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ===== Текст в поле ввода =====
static void drawInputText(HDC dc, const RECT &ri, int textL, int avail,
                          const wchar_t *s, int len, const wchar_t *placeholder,
                          bool showCaret) {
    SetBkMode(dc, TRANSPARENT);
    RECT rt = { textL, ri.top, textL + avail, ri.bottom };
    int caretX = textL;
    if (len == 0) {
        SelectObject(dc, gF.seg11i);
        SetTextColor(dc, CLR_MUTED);
        DrawTextW(dc, placeholder, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SelectObject(dc, gF.seg11);
        SetTextColor(dc, CLR_TEXT);
        SIZE sz;
        GetTextExtentPoint32W(dc, s, len, &sz);
        bool of = sz.cx > avail;
        DrawTextW(dc, s, len, &rt, (of ? DT_RIGHT : DT_LEFT) | DT_VCENTER | DT_SINGLELINE);
        caretX = textL + (of ? avail : sz.cx) + 1;
    }
    if (showCaret) {
        int mid = (ri.top + ri.bottom) / 2;
        fillRectColor(dc, caretX, mid - 10, caretX + 2, mid + 10, CLR_TEXT);
    }
}

// ===== Шапка (закреплена) =====
static void paintRing(HDC dc, Gdiplus::Graphics &g) {
    // === Взвешенный прогресс ===
    // Задачи (вес 3, пропорция), Практика (вес 1, пропорция от цели),
    // Языки (вес 1, пока не считаем).

    int done, total;
    getTodayStats(&done, &total);

    // === Вариант Б: фиксированный знаменатель = 5 ===
    // Задачи 3 + Практика 1 + Языки 1 = 5
    double score = 0.0;

    // Задачи (вес 3) — пропорция done/total × 3
    if (total > 0) {
        score += ((double)done / (double)total) * 3.0;
    }

    // Практика (вес 1) — пропорция от цели, БЕЗ обрезки
    if (practiceCount > 0) {
        int mins = practiceMinutesToday();
        double p = (double)mins / (double)g_practiceGoal;
        score += p;
    }

    // Языки (вес 1) — потом
    // if (languageCount > 0) { score += 0..1; }

    int pct = (int)((score * 100.0 / 5.0) + 0.5);

    float cx = (float)(WIN_W - UI_PAD - UI_RING_D / 2), cy = (float)UI_RING_CY;
    float rad = UI_RING_D / 2.0f - 3.0f;

    // === Внутреннее кольцо (0-100%) ===
    Gdiplus::Pen track(gc(CLR_DIVIDER), 6.0f);
    g.DrawEllipse(&track, cx - rad, cy - rad, rad * 2, rad * 2);

    int innerPct = (pct > 100) ? 100 : pct;
    if (innerPct > 0) {
        Gdiplus::LinearGradientBrush br(Gdiplus::PointF(cx, cy - rad - 3),
            Gdiplus::PointF(cx, cy + rad + 3), gc(CLR_ACCENT3), gc(CLR_ACCENT2));
        Gdiplus::Pen pen(&br, 6.0f);
        if (innerPct >= 100) {
            g.DrawEllipse(&pen, cx - rad, cy - rad, rad * 2, rad * 2);
        } else {
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawArc(&pen, cx - rad, cy - rad, rad * 2, rad * 2,
                      -90.0f, 360.0f * innerPct / 100.0f);
        }
    }

    // === Внешнее кольцо (перевыполнение > 100%) ===
    if (pct > 100) {
        float radOut = rad + 10.0f;
        Gdiplus::Pen trackOut(gc(CLR_DIVIDER), 4.0f);
        g.DrawEllipse(&trackOut, cx - radOut, cy - radOut, radOut * 2, radOut * 2);

        int outerPct = pct - 100;
        if (outerPct > 100) outerPct = 100;

        Gdiplus::Pen penOut(gc(CLR_ACCENT), 4.0f);
        if (outerPct >= 100) {
            g.DrawEllipse(&penOut, cx - radOut, cy - radOut, radOut * 2, radOut * 2);
        } else {
            penOut.SetStartCap(Gdiplus::LineCapRound);
            penOut.SetEndCap(Gdiplus::LineCapRound);
            g.DrawArc(&penOut, cx - radOut, cy - radOut, radOut * 2, radOut * 2,
                      -90.0f, 360.0f * outerPct / 100.0f);
        }
    }
    g.Flush(Gdiplus::FlushIntentionSync);

    wchar_t buf[8];
    swprintf(buf, 8, L"%d%%", pct);
    SetBkMode(dc, TRANSPARENT);
    // Цифра процента — крупный Inter Bold
    SelectObject(dc, gF.seg11);
    SetTextColor(dc, CLR_TEXT);
    RECT r1 = { (int)cx - 38, (int)cy - 24, (int)cx + 38, (int)cy + 6 };
    DrawTextW(dc, buf, -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    // Подпись — мелкий Segoe
    SelectObject(dc, gF.seg8);
    SetTextColor(dc, CLR_MUTED);
    RECT r2 = { (int)cx - 38, (int)cy + 8, (int)cx + 38, (int)cy + 24 };
    DrawTextW(dc, L"выполнено", -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paintStreak(HDC dc, Gdiplus::Graphics &g) {
    if (g_streak <= 0) return;

    wchar_t txt[32];
    swprintf(txt, 32, L"%d %ls", g_streak, dayWord(g_streak));
    SelectObject(dc, gF.seg10);
    SIZE sz;
    GetTextExtentPoint32W(dc, txt, (int)wcslen(txt), &sz);

    int w = 42 + sz.cx, h = UI_STREAK_H;
    int x = WIN_W - UI_PAD - w, y = UI_STREAK_Y;

    gFillRound(g, (float)x, (float)y, (float)w, (float)h, 12, CLR_SURFACE);
    if (g_streak >= 3) {
        static const BYTE alpha[3] = { 110, 55, 28 };
        for (int i = 0; i < 3; i++) {
            Gdiplus::GraphicsPath p;
            roundRectPath(p, (float)(x - i), (float)(y - i), (float)(w + 2 * i), (float)(h + 2 * i), (float)(12 + i));
            Gdiplus::Pen glow(gc(CLR_ACCENT2, alpha[i]), 1.5f);
            g.DrawPath(&glow, &p);
        }
    }
    gFlame(g, x + 10.0f, y + 4.0f);
    g.Flush(Gdiplus::FlushIntentionSync);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, CLR_TEXT);
    RECT tr = { x + 30, y, x + w, y + h };
    DrawTextW(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void paintWeekStrip(HDC dc, Gdiplus::Graphics &g) {
    static const wchar_t *wk[7] = { L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс" };
    int today = todayWeekIdx();
    int cellW = (WIN_W - UI_PAD * 2) / 7;
    for (int i = 0; i < 7; i++)
        if (i == today)
            gFillRound(g, (float)(UI_PAD + i * cellW + 4), (float)UI_STRIP_Y,
                       (float)(cellW - 8), (float)UI_STRIP_H, 12, CLR_SURFACE);
    g.Flush(Gdiplus::FlushIntentionSync);

    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.seg9);
    for (int i = 0; i < 7; i++) {
        SetTextColor(dc, i == today ? CLR_TEXT : CLR_MUTED);
        RECT r = { UI_PAD + i * cellW, UI_STRIP_Y, UI_PAD + (i + 1) * cellW, UI_STRIP_Y + UI_STRIP_H };
        DrawTextW(dc, wk[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// ===== Пилюли дней =====
static RECT rcDayPill(int i) {
    int arrowW = 30;
    int availW = WIN_W - UI_PAD * 2 - arrowW * 2 - 12;
    int cellW = availW / 7;
    int x = UI_PAD + arrowW + 6 + i * cellW;
    RECT r = { x, UI_DAY_PILL_Y, x + cellW - 2, UI_DAY_PILL_Y + UI_DAY_PILL_H };
    return r;
}
static RECT rcDayPrev() {
    RECT r = { UI_PAD, UI_DAY_PILL_Y, UI_PAD + 30, UI_DAY_PILL_Y + UI_DAY_PILL_H };
    return r;
}
static RECT rcDayNext() {
    RECT r = { WIN_W - UI_PAD - 30, UI_DAY_PILL_Y, WIN_W - UI_PAD, UI_DAY_PILL_Y + UI_DAY_PILL_H };
    return r;
}

static void getPillDate(int i, wchar_t *buf, int *dayNum) {
    int viewDay = viewDayNum();
    time_t t0 = time(NULL);
    struct tm *ti = localtime(&t0);
    int today = daysFromCivil(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
    int diff = viewDay - today;
    t0 += (time_t)diff * 86400;
    ti = localtime(&t0);
    int wday = (ti->tm_wday + 6) % 7;
    int mondayOffset = -wday;
    t0 += (time_t)(mondayOffset + i) * 86400;
    ti = localtime(&t0);
    if (buf) swprintf(buf, 11, L"%02d.%02d.%04d",
             ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
    if (dayNum) *dayNum = daysFromCivil(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
}

static const wchar_t *shortDayName(int i) {
    static const wchar_t *names[7] = { L"ПН", L"ВТ", L"СР", L"ЧТ", L"ПТ", L"СБ", L"ВС" };
    return names[i];
}

static void paintDayPills(HDC dc, Gdiplus::Graphics &g) {
    SetBkMode(dc, TRANSPARENT);

    SelectObject(dc, gF.seg11);
    bool hovPrev = (g_hover.kind == H_DAY_PREV);
    bool hovNext = (g_hover.kind == H_DAY_NEXT);
    SetTextColor(dc, hovPrev ? CLR_TEXT : CLR_ACCENT);
    RECT rp = rcDayPrev();
    DrawTextW(dc, L"<", -1, &rp, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(dc, hovNext ? CLR_TEXT : CLR_ACCENT);
    RECT rn = rcDayNext();
    DrawTextW(dc, L">", -1, &rn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int viewDay = viewDayNum();
    int todayNum = todayDayNumber();

    for (int i = 0; i < 7; i++) {
        RECT r = rcDayPill(i);
        int pillDay = 0;
        getPillDate(i, NULL, &pillDay);

        bool isCurrent = (pillDay == viewDay);
        bool isToday = (pillDay == todayNum);
        bool hov = (g_hover.kind == H_DAY_PILL && g_hover.idx == i);

        if (isCurrent) {
            gFillRound(g, (float)r.left, (float)r.top,
                       (float)(r.right - r.left), (float)(r.bottom - r.top), 6,
                       CLR_SURFACE);
        } else if (hov) {
            gFillRound(g, (float)r.left, (float)r.top,
                       (float)(r.right - r.left), (float)(r.bottom - r.top), 6,
                       CLR_SURFACE_HOV);
        }
        g.Flush(Gdiplus::FlushIntentionSync);

        // День недели
        SelectObject(dc, gF.seg8);
        SetTextColor(dc, isCurrent ? CLR_ACCENT : CLR_MUTED);
        RECT r1 = { r.left, r.top + 2, r.right, r.top + 14 };
        DrawTextW(dc, shortDayName(i), -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Число
        time_t t0 = time(NULL);
        struct tm *ti = localtime(&t0);
        int today = daysFromCivil(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
        int diff = pillDay - today;
        t0 += (time_t)diff * 86400;
        ti = localtime(&t0);

        wchar_t num[8];
        swprintf(num, 8, L"%d", ti->tm_mday);
        SelectObject(dc, gF.seg9);
        SetTextColor(dc, isCurrent ? CLR_TEXT : (isToday ? CLR_ACCENT : CLR_MUTED));
        RECT r2 = { r.left, r.top + 13, r.right, r.bottom };
        DrawTextW(dc, num, -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void paintHeader(HDC dc, Gdiplus::Graphics &g) {
    fillRectColor(dc, 0, 0, WIN_W, UI_HEADER_H, CLR_BG);

    paintRing(dc, g);
    paintStreak(dc, g);
    paintDayPills(dc, g);

    wchar_t d[64];
    getHeaderDate(d, 64);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT r1 = { UI_PAD, 12, 420, 32 };
    DrawTextW(dc, d, -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Иконка календаря
    {
        RECT ic = rcCalendarIcon();
        bool ihover = (g_hover.kind == H_CALENDAR_ICON);
        COLORREF col = ihover ? CLR_ACCENT : CLR_MUTED;
        // Квадрат-календарь: рамка + верхняя полоска + сетка
        Gdiplus::Pen pen(gc(col), 1.0f);
        float x = (float)ic.left, yy = (float)ic.top;
        float sz = (float)(ic.right - ic.left);
        g.DrawRectangle(&pen, x, yy, sz, sz);
        g.DrawLine(&pen, x, yy + sz * 0.3f, x + sz, yy + sz * 0.3f);
        // Точки-сетка внутри
        for (int gy = 0; gy < 2; gy++) {
            for (int gx = 0; gx < 3; gx++) {
                float dx = x + sz * (0.25f + gx * 0.25f);
                float dy = yy + sz * (0.55f + gy * 0.2f);
                Gdiplus::SolidBrush dotBrush(gc(col));
                g.FillEllipse(&dotBrush, dx - 0.8f, dy - 0.8f, 1.6f, 1.6f);
            }
        }
        g.Flush(Gdiplus::FlushIntentionSync);
    }

    SelectObject(dc, gF.disp36);
    SetTextColor(dc, CLR_TEXT);
    RECT r2 = { UI_PAD, 32, 430, 96 };
    DrawTextW(dc, L"FOCUS BITCH.", -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT r3 = { UI_PAD, 96, 430, 116 };
    DrawTextW(dc, L"follow the plan, not the mood.", -1, &r3,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    paintTabs(dc, g);

    fillRectColor(dc, 0, UI_HEADER_H - 1, WIN_W, UI_HEADER_H, CLR_DIVIDER);
}
// ===== Трекер привычек =====
static void paintHabits(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    int today = todayWeekIdx();
    float rowW = (float)(WIN_W - UI_PAD * 2);

    for (int i = 0; i < habitCount; i++) {
        int top = sy(L.habRows + i * HAB_ROW_H);
        if (top + HAB_ROW_H < UI_BODY_TOP || top > g_viewH) continue;
        float cy = top + HAB_ROW_H / 2.0f;
        bool rowHov = isHabHover(i);

        if (rowHov) gFillRound(g, (float)UI_PAD, (float)top + 2, rowW, HAB_ROW_H - 4.0f, 12, CLR_SURFACE);
        else if (i < habitCount - 1) gHLine(g, UI_PAD + 16.0f, (float)top + HAB_ROW_H - 1, rowW - 32.0f, CLR_DIVIDER);

        for (int k = 0; k < 7; k++) {
            float cx = habCx(k);
            bool hk = (g_hover.kind == H_HAB_CHECK && g_hover.idx == i && g_hover.idx2 == k);
            if (habits[i].done[k]) {
                gDot(g, cx, cy, 8, hk ? mixColor(CLR_ACCENT3, RGB(255, 255, 255), 15) : CLR_ACCENT3);
                gCheck(g, cx, cy, 0.8f);
            } else {
                Gdiplus::Pen ring(gc(hk ? CLR_ACCENT3 : CLR_MUTED), 1.5f);
                g.DrawEllipse(&ring, cx - 7.25f, cy - 7.25f, 14.5f, 14.5f);
            }
        }
        bool hd = (g_hover.kind == H_HAB_DEL && g_hover.idx == i);
        gCross(g, HAB_DEL_CX, cy, 4.0f, hd ? CLR_ACCENT : CLR_MUTED);
    }
    g.Flush(Gdiplus::FlushIntentionSync);

    SetBkMode(dc, TRANSPARENT);
    int ty = sy(L.habTitle);
    SelectObject(dc, gF.seg9);
    SetTextColor(dc, CLR_MUTED);
    RECT rt = { UI_PAD, ty, 300, ty + 36 };
    DrawTextW(dc, L"ПРИВЫЧКИ", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    static const wchar_t *wk[7] = { L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс" };
    SelectObject(dc, gF.seg8);
    for (int k = 0; k < 7; k++) {
        SetTextColor(dc, k == today ? CLR_TEXT : CLR_MUTED);
        int cx = (int)habCx(k);
        RECT rl = { cx - 14, ty, cx + 14, ty + 36 };
        DrawTextW(dc, wk[k], -1, &rl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (habitCount == 0) {
        int top = sy(L.habRows);
        SelectObject(dc, gF.seg11i);
        SetTextColor(dc, CLR_MUTED);
        RECT re = { UI_PAD + 16, top, WIN_W - UI_PAD, top + HAB_ROW_H };
        DrawTextW(dc, L"Привычек пока нет — добавь первую", -1, &re, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(dc, gF.seg11);
    SetTextColor(dc, CLR_TEXT);
    for (int i = 0; i < habitCount; i++) {
        int top = sy(L.habRows + i * HAB_ROW_H);
        if (top + HAB_ROW_H < UI_BODY_TOP || top > g_viewH) continue;
        RECT rn = { UI_PAD + 16, top, 294, top + HAB_ROW_H };
        DrawTextW(dc, habits[i].name, -1, &rn, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if (g_habAdding) {
        RECT ri = rcHabInput(L), ro = rcHabOk(L);
        gFillRound(g, (float)ri.left, (float)ri.top, (float)(ri.right - ri.left), 36.0f, 12, CLR_INPUT);
        gStrokeRound(g, ri.left + 0.5f, ri.top + 0.5f, (float)(ri.right - ri.left) - 1, 35.0f, 12, CLR_ACCENT, 1.0f);
        g.Flush(Gdiplus::FlushIntentionSync);
        drawInputText(dc, ri, ri.left + 16, ri.right - ri.left - 32, g_hInput, g_hInputLen,
                      L"Название привычки...", g_focus == FOCUS_HABIT && g_caretOn);
        paintAccentButton(dc, g, ro, L"Готово", g_hover.kind == H_HAB_OK, false);
    } else {
        paintAccentButton(dc, g, rcHabAdd(L), L"Добавить привычку", g_hover.kind == H_HAB_ADDBTN, true);
    }
}

// ===== Задачи =====
static void paintTaskHeader(HDC dc, const Layout &L) {
    int ty = sy(L.taskTitle);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.seg9);
    SetTextColor(dc, CLR_MUTED);
    RECT rt = { UI_PAD, ty, 300, ty + 36 };
    DrawTextW(dc, L"ЗАДАЧИ", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void paintInput(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    RECT ri = rcInput(L);
    float w = (float)(ri.right - ri.left), h = (float)UI_INPUT_H;
    bool hov = (g_hover.kind == H_INPUT || g_hover.kind == H_PRIO);

    gFillRound(g, (float)ri.left, (float)ri.top, w, h, 12, CLR_INPUT);
    gStrokeRound(g, ri.left + 0.5f, ri.top + 0.5f, w - 1, h - 1, 12, hov ? CLR_MUTED : CLR_DIVIDER, 1.0f);

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

    int textL = ri.left + 18;
    int avail = (int)prioCx(0) - 11 - 12 - textL;
    drawInputText(dc, ri, textL, avail, g_input, g_inputLen, L"Что нужно сделать...",
                  g_focus == FOCUS_TASK && g_caretOn);
}

static void paintButtons(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    paintAccentButton(dc, g, rcBtn(L, 0), L"Добавить", g_hover.kind == H_ADD, true);
    paintSurfaceButton(dc, g, rcBtn(L, 1), L"Удалить", g_hover.kind == H_DEL,
                       g_selected >= 0 ? CLR_TEXT : CLR_MUTED);
    paintSurfaceButton(dc, g, rcBtn(L, 2), L"Свернуть", g_hover.kind == H_HIDE, CLR_TEXT);
}

static void paintEmpty(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    // Пустое состояние — только типографика, без иллюстраций
    int cy = sy(L.listY) + 80;

    // Верхняя тонкая линия
    gFillRound(g, (float)(WIN_W/2 - 12), (float)cy, 24.0f, 1.0f, 0.5f, CLR_DIVIDER);

    // Главная фраза
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_TEXT);
    RECT r1 = { 0, cy + 20, WIN_W, cy + 60 };
    DrawTextW(dc, L"just start.", -1, &r1,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Подпись
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT r2 = { 0, cy + 60, WIN_W, cy + 90 };
    DrawTextW(dc, L"добавь первую запись", -1, &r2,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Нижняя тонкая линия
    gFillRound(g, (float)(WIN_W/2 - 12), (float)(cy + 100), 24.0f, 1.0f, 0.5f, CLR_DIVIDER);

    g.Flush(Gdiplus::FlushIntentionSync);
}

static void paintList(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    if (count == 0) { paintEmpty(dc, g, L); return; }

    int topCy = g_scroll - L.listY;
    int botCy = g_scroll + viewBodyH() - L.listY;
    int first = topCy > 0 ? topCy / UI_ROW : 0;
    int last  = botCy >= 0 ? botCy / UI_ROW : -1;
    if (last > count - 1) last = count - 1;
    float cw = (float)(WIN_W - UI_PAD * 2);

    for (int r = first; r <= last; r++) {
        int idx = count - 1 - r;
        float y = (float)sy(L.listY + r * UI_ROW);
        bool sel  = (g_selected == idx);
        bool hov  = (g_hover.idx == idx && (g_hover.kind == H_CARD || g_hover.kind == H_CHECK));
        bool hchk = (g_hover.kind == H_CHECK && g_hover.idx == idx);

        gFillRound(g, (float)UI_PAD, y, cw, (float)UI_CARD_H, 12, hov ? CLR_SURFACE_HOV : CLR_SURFACE);
        if (sel) gStrokeRound(g, UI_PAD + 0.5f, y + 0.5f, cw - 1, UI_CARD_H - 1.0f, 12, CLR_MUTED, 1.0f);

        float cx = UI_PAD + 26.0f, cy = y + UI_CARD_H / 2.0f;
        if (diary[idx].done) {
            gDot(g, cx, cy, 10, hchk ? mixColor(CLR_ACCENT3, RGB(255, 255, 255), 15) : CLR_ACCENT3);
            gCheck(g, cx, cy, 1.0f);
        } else {
            Gdiplus::Pen ring(gc(hchk ? CLR_ACCENT3 : CLR_MUTED), 1.5f);
            g.DrawEllipse(&ring, cx - 9.25f, cy - 9.25f, 18.5f, 18.5f);
        }
        int p = diary[idx].priority;
        if (p < 1 || p > 3) p = 2;
        gDot(g, cx + 24.0f, cy, 4, CLR_PRIO[p - 1]);
    }
    g.Flush(Gdiplus::FlushIntentionSync);

    SetBkMode(dc, TRANSPARENT);
    for (int r = first; r <= last; r++) {
        int idx = count - 1 - r;
        int y = sy(L.listY + r * UI_ROW);
        bool done = diary[idx].done;

        SelectObject(dc, gF.seg9);
        SetTextColor(dc, CLR_MUTED);
        RECT rd = { 94, y, 164, y + UI_CARD_H };
        DrawTextW(dc, diary[idx].date, -1, &rd, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dc, done ? gF.seg11s : gF.seg11);
        SetTextColor(dc, done ? CLR_MUTED : CLR_TEXT);
        RECT rt = { 172, y, WIN_W - UI_PAD - 16, y + UI_CARD_H };
        DrawTextW(dc, diary[idx].text, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

// ===== Табы (навигация) =====
static RECT rcTab(int i) {
    // 5 табов, распределены по ширине
    int totalW = WIN_W - UI_PAD * 2;
    int tabW = totalW / TAB_COUNT;
    int x = UI_PAD + i * tabW;
    RECT r = { x, UI_TABS_Y, x + tabW, UI_TABS_Y + UI_TABS_H };
    return r;
}

static void paintTabs(HDC dc, Gdiplus::Graphics &g) {
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, gF.seg9);

    for (int i = 0; i < TAB_COUNT; i++) {
        RECT r = rcTab(i);
        bool active = (i == g_activeTab);
        bool hover  = (g_hover.kind == H_TAB && g_hover.idx == i);

        COLORREF col = active ? CLR_ACCENT : (hover ? CLR_TEXT : CLR_MUTED);
        SetTextColor(dc, col);

        DrawTextW(dc, TAB_NAMES[i], -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (active) {
            // Подчёркивание активного таба
            int textW = 0;
            SIZE sz;
            GetTextExtentPoint32W(dc, TAB_NAMES[i], (int)wcslen(TAB_NAMES[i]), &sz);
            textW = sz.cx;
            int ux = (r.left + r.right) / 2 - textW / 2;
            int uy = r.bottom - 2;
            gFillRound(g, (float)ux, (float)uy, (float)textW, 1.0f, 0.5f, CLR_ACCENT);
        }
    }
    g.Flush(Gdiplus::FlushIntentionSync);
}

static Hit hitTestTabs(int x, int y) {
    Hit h = { H_NONE, -1, -1 };
    if (y < UI_TABS_Y || y >= UI_TABS_Y + UI_TABS_H) return h;
    for (int i = 0; i < TAB_COUNT; i++) {
        RECT r = rcTab(i);
        if (x >= r.left && x < r.right) {
            h.kind = H_TAB;
            h.idx = i;
            return h;
        }
    }
    return h;
}

// ===== Контент вкладок =====

// ===== Зоны для кликов на TODAY и в шапке =====
static RECT rcCalendarIcon() {
    // Иконка календаря рядом с датой (в шапке)
    // Дата рисуется с x=UI_PAD, y=12..32. Иконка справа от неё.
    int dateW = 200;  // примерная ширина текста даты
    int size = 18;
    RECT r = { UI_PAD + dateW + 8, 14, UI_PAD + dateW + 8 + size, 14 + size };
    return r;
}

static RECT rcTodayAddTask(const Layout &L) {
    int y = sy(16) + 52 + 40 + 40 + 60;  // после сводки
    RECT r = { UI_PAD, y, WIN_W - UI_PAD, y + 28 };
    return r;
}

static RECT rcTodayAddPractice(const Layout &L) {
    int y = sy(16) + 52 + 40 + 40 + 60 + 28;
    RECT r = { UI_PAD, y, WIN_W - UI_PAD, y + 28 };
    return r;
}

static RECT rcTodayAddWord(const Layout &L) {
    int y = sy(16) + 52 + 40 + 40 + 60 + 28 + 28;
    RECT r = { UI_PAD, y, WIN_W - UI_PAD, y + 28 };
    return r;
}

static void paintToday(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    SetBkMode(dc, TRANSPARENT);
    int y = sy(16);

    // Заголовок "СЕГОДНЯ"
    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_ACCENT);
    RECT r1 = { UI_PAD, y, WIN_W - UI_PAD, y + 40 };
    DrawTextW(dc, L"СЕГОДНЯ", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 52;

    // Сводка
    SelectObject(dc, gF.seg11);
    SetTextColor(dc, CLR_TEXT);

    wchar_t buf[64];
    int done, total;
    getTodayStats(&done, &total);

    swprintf(buf, 64, L"%d из %d задач", done, total);
    RECT r2 = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
    DrawTextW(dc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 40;

    // Практика (реальные данные)
    int practiceMin = practiceMinutesToday();
    swprintf(buf, 64, L"%d минут практики", practiceMin);
    RECT r3 = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
    DrawTextW(dc, buf, -1, &r3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 40;

    // Языки (заглушка)
    RECT r4 = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
    DrawTextW(dc, L"0 языков сегодня", -1, &r4, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 60;

    // Быстрые действия (кликабельные)
    SelectObject(dc, gF.seg11);
    y = sy(16) + 52 + 40 + 40 + 60;

    bool h1 = (g_hover.kind == H_TODAY_ADD_TASK);
    bool h2 = (g_hover.kind == H_TODAY_ADD_PRACTICE);
    bool h3 = (g_hover.kind == H_TODAY_ADD_WORD);

    SetTextColor(dc, h1 ? CLR_ACCENT : CLR_TEXT);
    RECT r5 = { UI_PAD, y, WIN_W - UI_PAD, y + 28 };
    DrawTextW(dc, L"+ добавить задачу", -1, &r5, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 28;

    SetTextColor(dc, h2 ? CLR_ACCENT : CLR_TEXT);
    RECT r6 = { UI_PAD, y, WIN_W - UI_PAD, y + 28 };
    DrawTextW(dc, L"+ добавить практику", -1, &r6, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 28;

    SetTextColor(dc, h3 ? CLR_ACCENT : CLR_TEXT);
    RECT r7 = { UI_PAD, y, WIN_W - UI_PAD, y + 28 };
    DrawTextW(dc, L"+ добавить слово", -1, &r7, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 60;

    // "just start." внизу
    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_TEXT);
    RECT r8 = { 0, sy(L.contentH - 120), WIN_W, sy(L.contentH - 80) };
    DrawTextW(dc, L"just start.", -1, &r8, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT r9 = { 0, sy(L.contentH - 80), WIN_W, sy(L.contentH - 55) };
    DrawTextW(dc, L"добавь первую запись", -1, &r9, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paintHabitsTab(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    paintHabits(dc, g, L);
}

static void paintTasksTab(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    paintTaskHeader(dc, L);
    paintInput(dc, g, L);
    paintButtons(dc, g, L);
    paintList(dc, g, L);
}

// ===== Форма выбора языков =====
static void cancelLangForm() {
    g_langForm = false;
    for (int i = 0; i < LANG_TEMPLATES_COUNT; i++) g_langSelected[i] = false;
}

static void commitLangForm() {
    for (int i = 0; i < LANG_TEMPLATES_COUNT; i++) {
        if (!g_langSelected[i]) continue;
        if (languageCount >= MAX_LANGUAGES) break;
        bool exists = false;
        for (int j = 0; j < languageCount; j++) {
            if (wcscmp(languages[j].name, LANG_TEMPLATES[i].name) == 0) {
                exists = true; break;
            }
        }
        if (exists) continue;
        Language *lang = &languages[languageCount];
        lang->id = languageCount + 1;
        wcsncpy(lang->name, LANG_TEMPLATES[i].name, LANG_NAME_MAX - 1);
        lang->name[LANG_NAME_MAX - 1] = 0;
        wcsncpy(lang->goal, LANG_TEMPLATES[i].goal, LANG_GOAL_MAX - 1);
        lang->goal[LANG_GOAL_MAX - 1] = 0;
        lang->progress = LANG_TEMPLATES[i].progress;
        lang->wordsTotal = LANG_TEMPLATES[i].wordsTotal;
        lang->streakDays = LANG_TEMPLATES[i].streakDays;
        languageCount++;
        // Добавляем слова для нового языка
        addWordsForLanguage(lang->id, lang->name);
    }
    saveLanguages();
    cancelLangForm();
    g_openLang = -1;
}

static RECT rcLfRow(int i) {
    int baseY = sy(16) + 60;
    int rowH = 70;
    RECT r = { UI_PAD, baseY + i * rowH, WIN_W - UI_PAD, baseY + i * rowH + rowH - 8 };
    return r;
}
static RECT rcLfOkBtn() {
    int baseY = sy(16) + 60 + LANG_TEMPLATES_COUNT * 70 + 20;
    RECT r = { UI_PAD, baseY, UI_PAD + 200, baseY + 40 };
    return r;
}
static RECT rcLfCancelBtn() {
    int baseY = sy(16) + 60 + LANG_TEMPLATES_COUNT * 70 + 20;
    RECT r = { UI_PAD + 220, baseY, UI_PAD + 340, baseY + 40 };
    return r;
}

static void paintLangForm(HDC dc, Gdiplus::Graphics &g) {
    SetBkMode(dc, TRANSPARENT);

    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_ACCENT);
    RECT r1 = { UI_PAD, sy(16), WIN_W - UI_PAD, sy(16) + 40 };
    DrawTextW(dc, L"ВЫБЕРИ ЯЗЫКИ", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    for (int i = 0; i < LANG_TEMPLATES_COUNT; i++) {
        RECT r = rcLfRow(i);
        bool sel = g_langSelected[i];
        bool hov = (g_hover.kind == H_LF_ROW && g_hover.idx == i);

        if (hov) {
            gFillRound(g, (float)r.left, (float)r.top,
                       (float)(r.right - r.left), (float)(r.bottom - r.top), 8,
                       CLR_SURFACE_HOV);
        }

        int cbSize = 20;
        int cbX = r.left + 12;
        int cbY = r.top + (r.bottom - r.top) / 2 - cbSize / 2;
        if (sel) {
            gFillRound(g, (float)cbX, (float)cbY, (float)cbSize, (float)cbSize, 4, CLR_ACCENT);
            Gdiplus::Pen p(gc(CLR_BG), 2.0f);
            g.DrawLine(&p, (float)(cbX + 5), (float)(cbY + 10),
                           (float)(cbX + 9), (float)(cbY + 14));
            g.DrawLine(&p, (float)(cbX + 9), (float)(cbY + 14),
                           (float)(cbX + 16), (float)(cbY + 6));
        } else {
            gFillRound(g, (float)cbX, (float)cbY, (float)cbSize, (float)cbSize, 4, CLR_INPUT);
            Gdiplus::Pen p(gc(CLR_MUTED), 1.0f);
            g.DrawRectangle(&p, (float)cbX, (float)cbY, (float)cbSize, (float)cbSize);
        }
        g.Flush(Gdiplus::FlushIntentionSync);

        SelectObject(dc, gF.seg11);
        SetTextColor(dc, sel ? CLR_TEXT : CLR_MUTED);
        RECT rn = { cbX + cbSize + 14, r.top, r.right - 20, r.top + 30 };
        DrawTextW(dc, LANG_TEMPLATES[i].name, -1, &rn,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dc, gF.seg9);
        SetTextColor(dc, CLR_MUTED);
        wchar_t desc[128];
        swprintf(desc, 128, L"%ls · %d слов · %d дней",
                 LANG_TEMPLATES[i].goal,
                 LANG_TEMPLATES[i].wordsTotal,
                 LANG_TEMPLATES[i].streakDays);
        RECT rd = { cbX + cbSize + 14, r.top + 30, r.right - 20, r.bottom };
        DrawTextW(dc, desc, -1, &rd,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    RECT okR = rcLfOkBtn();
    gFillRound(g, (float)okR.left, (float)okR.top, 200.0f, 40.0f, 8, CLR_ACCENT);
    g.Flush(Gdiplus::FlushIntentionSync);
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_BG);
    DrawTextW(dc, L"ДОБАВИТЬ ВЫБРАННЫЕ", -1, &okR,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT ccR = rcLfCancelBtn();
    SetTextColor(dc, CLR_MUTED);
    DrawTextW(dc, L"Отмена", -1, &ccR,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paintLanguageTab(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    if (g_langForm) {
        paintLangForm(dc, g);
        return;
    }

    // === Сводка одного языка ===
    if (g_openLang >= 0 && g_openLang < languageCount) {
        Language *lang = &languages[g_openLang];
        SetBkMode(dc, TRANSPARENT);

        // Кнопка "← назад"
        SelectObject(dc, gF.seg11);
        bool hovBack = (g_hover.kind == H_LANG_BACK);
        SetTextColor(dc, hovBack ? CLR_TEXT : CLR_ACCENT);
        RECT rb = { UI_PAD, sy(16), UI_PAD + 100, sy(16) + 30 };
        DrawTextW(dc, L"← назад", -1, &rb, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        int y = sy(16) + 50;

        // Название языка — огромно
        SelectObject(dc, gF.disp36);
        SetTextColor(dc, CLR_TEXT);
        RECT rn = { UI_PAD, y, WIN_W - UI_PAD, y + 60 };
        DrawTextW(dc, lang->name, -1, &rn, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += 70;

        // Цель — крупно
        SelectObject(dc, gF.seg11);
        SetTextColor(dc, CLR_MUTED);
        RECT rg = { UI_PAD, y, WIN_W - UI_PAD, y + 26 };
        DrawTextW(dc, lang->goal, -1, &rg, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += 50;

        // Большая полоса прогресса
        int barW = WIN_W - UI_PAD * 2;
        int barH = 12;
        gFillRound(g, (float)UI_PAD, (float)y, (float)barW, (float)barH, 6, CLR_DIVIDER);
        int fill = barW * lang->progress / 100;
        if (fill > 0) {
            gFillRound(g, (float)UI_PAD, (float)y, (float)fill, (float)barH, 6, CLR_ACCENT);
        }
        g.Flush(Gdiplus::FlushIntentionSync);
        y += barH + 10;

        // Процент справа
        wchar_t buf[32];
        swprintf(buf, 32, L"%d%%", lang->progress);
        SelectObject(dc, gF.disp20);
        SetTextColor(dc, CLR_ACCENT);
        RECT rp = { WIN_W - UI_PAD - 100, y - barH - 10, WIN_W - UI_PAD, y - barH + 20 };
        DrawTextW(dc, buf, -1, &rp, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 30;

        // Статистика
        SelectObject(dc, gF.seg11);
        SetTextColor(dc, CLR_TEXT);
        swprintf(buf, 32, L"%d слов", lang->wordsTotal);
        RECT rw = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
        DrawTextW(dc, buf, -1, &rw, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dc, gF.seg10);
        SetTextColor(dc, CLR_MUTED);
        swprintf(buf, 32, L"%d дней streak", lang->streakDays);
        RECT rst = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
        DrawTextW(dc, buf, -1, &rst, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 50;

        // === Секция слов ===
        // Считаем слова этого языка
        int myLangId = lang->id;
        int wordCountForLang = 0;
        for (int i = 0; i < wordCount; i++) {
            if (words[i].languageId == myLangId) wordCountForLang++;
        }

        SelectObject(dc, gF.seg10);
        SetTextColor(dc, CLR_MUTED);
        wchar_t bufHdr[64];
        swprintf(bufHdr, 64, L"СЛОВА (%d)", wordCountForLang);
        RECT rsl = { UI_PAD, y, WIN_W - UI_PAD, y + 24 };
        DrawTextW(dc, bufHdr, -1, &rsl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += 32;

        if (wordCountForLang == 0) {
            SelectObject(dc, gF.seg11);
            SetTextColor(dc, CLR_MUTED);
            RECT rse = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
            DrawTextW(dc, L"слова пока не добавлены", -1, &rse,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {
            // Отрисовка слов
            int shown = 0;
            for (int i = 0; i < wordCount; i++) {
                if (words[i].languageId != myLangId) continue;

                // Оригинал — крупно
                SelectObject(dc, gF.seg11);
                SetTextColor(dc, CLR_TEXT);
                RECT ro = { UI_PAD, y, WIN_W - UI_PAD, y + 26 };
                DrawTextW(dc, words[i].original, -1, &ro,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                y += 26;

                // Транскрипция · перевод — мелким
                SelectObject(dc, gF.seg9);
                SetTextColor(dc, CLR_MUTED);
                wchar_t bufWord[128];
                swprintf(bufWord, 128, L"%ls · %ls",
                         words[i].transcription, words[i].translation);
                RECT rw = { UI_PAD, y, WIN_W - UI_PAD, y + 20 };
                DrawTextW(dc, bufWord, -1, &rw,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                y += 30;

                shown++;
            }
        }

        return;
    }

    // === Список языков ===
    SetBkMode(dc, TRANSPARENT);
    int y = sy(16);

    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_ACCENT);
    RECT r1 = { UI_PAD, y, WIN_W - UI_PAD, y + 40 };
    DrawTextW(dc, L"ЯЗЫКИ", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 56;

    if (languageCount == 0) {
        SelectObject(dc, gF.seg11);
        SetTextColor(dc, CLR_MUTED);
        RECT r2 = { UI_PAD, y + 20, WIN_W - UI_PAD, y + 50 };
        DrawTextW(dc, L"пока нет языков", -1, &r2,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, gF.seg10);
        RECT r3 = { UI_PAD, y + 50, WIN_W - UI_PAD, y + 78 };
        DrawTextW(dc, L"добавь первый — английский, армянский,", -1, &r3,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT r4 = { UI_PAD, y + 78, WIN_W - UI_PAD, y + 106 };
        DrawTextW(dc, L"грузинский, китайский или свой", -1, &r4,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += 130;
    } else {
        for (int i = 0; i < languageCount; i++) {
            Language *lang = &languages[i];
            bool hov = (g_hover.kind == H_LANG_OPEN && g_hover.idx == i);

            // Фон при hover
            if (hov) {
                gFillRound(g, (float)(UI_PAD - 6), (float)(y - 4),
                           (float)(WIN_W - (UI_PAD - 6) * 2), 110.0f, 8,
                           CLR_SURFACE_HOV);
            }

            // Название языка
            SelectObject(dc, gF.disp20);
            SetTextColor(dc, hov ? CLR_ACCENT : CLR_TEXT);
            RECT rn = { UI_PAD, y, WIN_W - UI_PAD, y + 34 };
            DrawTextW(dc, lang->name, -1, &rn, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            y += 36;

            // Цель
            SelectObject(dc, gF.seg10);
            SetTextColor(dc, CLR_MUTED);
            RECT rg = { UI_PAD, y, WIN_W - UI_PAD - 30, y + 22 };
            DrawTextW(dc, lang->goal, -1, &rg, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // × для удаления (справа сверху)
            bool hovX = (g_hover.kind == H_LANG_DEL && g_hover.idx == i);
            SetTextColor(dc, hovX ? CLR_ACCENT : CLR_MUTED);
            RECT rx = { WIN_W - UI_PAD - 24, y - 4, WIN_W - UI_PAD, y + 22 };
            DrawTextW(dc, L"×", -1, &rx, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            y += 26;

            // Прогресс-бар
            int barW = WIN_W - UI_PAD * 2;
            gFillRound(g, (float)UI_PAD, (float)y, (float)barW, 6.0f, 3, CLR_DIVIDER);
            int fill = barW * lang->progress / 100;
            if (fill > 0) {
                gFillRound(g, (float)UI_PAD, (float)y, (float)fill, 6.0f, 3, CLR_ACCENT);
            }
            g.Flush(Gdiplus::FlushIntentionSync);
            y += 14;

            // Слова + streak + %
            SelectObject(dc, gF.seg9);
            SetTextColor(dc, CLR_MUTED);
            wchar_t buf[64];
            swprintf(buf, 64, L"%d слов · %d дней", lang->wordsTotal, lang->streakDays);
            RECT rs = { UI_PAD, y, WIN_W - UI_PAD - 60, y + 20 };
            DrawTextW(dc, buf, -1, &rs, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            swprintf(buf, 64, L"%d%%", lang->progress);
            SetTextColor(dc, CLR_ACCENT);
            RECT rp = { WIN_W - UI_PAD - 60, y, WIN_W - UI_PAD, y + 20 };
            DrawTextW(dc, buf, -1, &rp, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            y += 40;
        }
    }

    // Кнопка "+ добавить язык"
    SelectObject(dc, gF.seg11);
    bool hovAdd = (g_hover.kind == H_LANG_ADD);
    SetTextColor(dc, hovAdd ? CLR_TEXT : CLR_ACCENT);
    RECT rAdd = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
    DrawTextW(dc, L"+ добавить язык", -1, &rAdd,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// ===== Форма добавления практики =====
static void cancelPracticeForm() {
    g_practiceForm = false;
    g_practiceTypeInput[0] = 0; g_practiceTypeLen = 0;
    g_practiceMinInput[0] = 0; g_practiceMinLen = 0;
    g_practiceField = 0;
}

static void commitPracticeForm() {
    if (g_practiceTypeLen == 0) { cancelPracticeForm(); return; }
    int mins = _wtoi(g_practiceMinInput);
    if (mins <= 0) mins = 30;
    if (practiceCount < MAX_PRACTICES) {
        Practice *p = &practices[practiceCount];
        p->id = practiceCount + 1;
        getCurrentDate(p->date);
        wcsncpy(p->type, g_practiceTypeInput, PRACT_NAME_MAX - 1);
        p->type[PRACT_NAME_MAX - 1] = 0;
        p->minutes = mins;
        practiceCount++;
        savePractices();
    }
    cancelPracticeForm();
}

// ===== Зоны формы PRACTICE (единая база) =====
// Схема:
// baseY+0   .. +40  : заголовок "НОВАЯ ПРАКТИКА"
// baseY+60  .. +80  : label "ТИП ПРАКТИКИ"
// baseY+82  .. +122 : поле ввода типа
// baseY+134 .. +156 : label "БЫСТРЫЙ ВЫБОР"
// baseY+158 .. +234 : пилюли (2 ряда по 4)
// baseY+246 .. +266 : label "МИНУТЫ"
// baseY+268 .. +308 : поле минут
// baseY+328 .. +364 : кнопки ДОБАВИТЬ/Отмена

static RECT rcPfTypeBox() {
    int y = sy(16) + 82;
    RECT r = { UI_PAD, y, WIN_W - UI_PAD, y + 40 };
    return r;
}
static RECT rcPfMinBox() {
    int y = sy(16) + 268;
    RECT r = { UI_PAD, y, UI_PAD + 120, y + 40 };
    return r;
}
static RECT rcPfOkBtn() {
    int y = sy(16) + 328;
    RECT r = { UI_PAD, y, UI_PAD + 140, y + 36 };
    return r;
}
static RECT rcPfCancelBtn() {
    int y = sy(16) + 328;
    RECT r = { UI_PAD + 160, y, UI_PAD + 280, y + 36 };
    return r;
}
static RECT rcPfPill(int i) {
    int y0 = sy(16) + 158;
    int row = i / 4;
    int col = i % 4;
    int pillW = (WIN_W - UI_PAD * 2 - 18) / 4;
    int x = UI_PAD + col * (pillW + 6);
    int y = y0 + row * 38;
    RECT r = { x, y, x + pillW, y + 32 };
    return r;
}

static void paintPracticeForm(HDC dc, Gdiplus::Graphics &g) {
    SetBkMode(dc, TRANSPARENT);

    // Заголовок
    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_ACCENT);
    RECT r1 = { UI_PAD, sy(16), WIN_W - UI_PAD, sy(16) + 40 };
    DrawTextW(dc, L"НОВАЯ ПРАКТИКА", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Label "ТИП ПРАКТИКИ"
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT rlbl = { UI_PAD, sy(16) + 60, WIN_W - UI_PAD, sy(16) + 80 };
    DrawTextW(dc, L"ТИП ПРАКТИКИ", -1, &rlbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Поле ввода типа
    RECT rb = rcPfTypeBox();
    bool typeActive = (g_practiceField == 0);
    gFillRound(g, (float)rb.left, (float)rb.top, (float)(rb.right - rb.left), 40.0f, 8, CLR_INPUT);
    gStrokeRound(g, rb.left + 0.5f, rb.top + 0.5f, (float)(rb.right - rb.left) - 1, 39.0f, 8,
                 typeActive ? CLR_ACCENT : CLR_DIVIDER, 1.0f);
    g.Flush(Gdiplus::FlushIntentionSync);

    SelectObject(dc, gF.seg11);
    RECT rt = { rb.left + 14, rb.top, rb.right - 14, rb.bottom };
    if (g_practiceTypeLen == 0) {
        SetTextColor(dc, CLR_MUTED);
        DrawTextW(dc, L"Yoga, Плавание, Танцы...", -1, &rt,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(dc, CLR_TEXT);
        DrawTextW(dc, g_practiceTypeInput, -1, &rt,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Label "БЫСТРЫЙ ВЫБОР"
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT rbs = { UI_PAD, sy(16) + 134, WIN_W - UI_PAD, sy(16) + 156 };
    DrawTextW(dc, L"БЫСТРЫЙ ВЫБОР", -1, &rbs, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Пилюли
    SelectObject(dc, gF.seg9);
    for (int i = 0; i < PRACTICE_TYPES_COUNT; i++) {
        RECT rp = rcPfPill(i);
        bool hov = (g_hover.kind == H_PF_PILL && g_hover.idx == i);
        gFillRound(g, (float)rp.left, (float)rp.top,
                   (float)(rp.right - rp.left), 32.0f, 16,
                   hov ? CLR_SURFACE_HOV : CLR_SURFACE);
        SetTextColor(dc, hov ? CLR_ACCENT : CLR_TEXT);
        DrawTextW(dc, PRACTICE_TYPES[i], -1, &rp,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    g.Flush(Gdiplus::FlushIntentionSync);

    // Label "МИНУТЫ"
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT rml = { UI_PAD, sy(16) + 246, WIN_W - UI_PAD, sy(16) + 266 };
    DrawTextW(dc, L"МИНУТЫ", -1, &rml, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Поле минут
    RECT rmb = rcPfMinBox();
    bool minActive = (g_practiceField == 1);
    gFillRound(g, (float)rmb.left, (float)rmb.top, 120.0f, 40.0f, 8, CLR_INPUT);
    gStrokeRound(g, rmb.left + 0.5f, rmb.top + 0.5f, 119.0f, 39.0f, 8,
                 minActive ? CLR_ACCENT : CLR_DIVIDER, 1.0f);
    g.Flush(Gdiplus::FlushIntentionSync);

    SelectObject(dc, gF.seg11);
    SetTextColor(dc, CLR_TEXT);
    RECT rmv = { rmb.left + 14, rmb.top, rmb.right - 14, rmb.bottom };
    if (g_practiceMinLen == 0) {
        SetTextColor(dc, CLR_MUTED);
        DrawTextW(dc, L"30", -1, &rmv, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        DrawTextW(dc, g_practiceMinInput, -1, &rmv, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Кнопки
    RECT okR = rcPfOkBtn();
    gFillRound(g, (float)okR.left, (float)okR.top, 140.0f, 36.0f, 8, CLR_ACCENT);
    g.Flush(Gdiplus::FlushIntentionSync);
    SelectObject(dc, gF.seg11);
    SetTextColor(dc, CLR_BG);
    DrawTextW(dc, L"ДОБАВИТЬ", -1, &okR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT ccR = rcPfCancelBtn();
    SetTextColor(dc, CLR_MUTED);
    DrawTextW(dc, L"Отмена", -1, &ccR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paintPracticeTab(HDC dc, Gdiplus::Graphics &g, const Layout &L) {
    if (g_practiceForm) {
        paintPracticeForm(dc, g);
        return;
    }
    SetBkMode(dc, TRANSPARENT);
    int y = sy(16);

    // Заголовок "ПРАКТИКА"
    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_ACCENT);
    RECT r1 = { UI_PAD, y, WIN_W - UI_PAD, y + 40 };
    DrawTextW(dc, L"ПРАКТИКА", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 56;

    // Сегодня: X мин
    int mins = practiceMinutesToday();
    wchar_t buf[64];
    swprintf(buf, 64, L"Сегодня: %d мин", mins);
    SelectObject(dc, gF.disp20);
    SetTextColor(dc, CLR_TEXT);
    RECT r2 = { UI_PAD, y, WIN_W - UI_PAD, y + 36 };
    DrawTextW(dc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 42;

    // Streak
    int st = practiceStreak();
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    swprintf(buf, 64, L"%d %ls подряд", st, dayWord(st));
    RECT r3 = { UI_PAD, y, WIN_W - UI_PAD, y + 24 };
    DrawTextW(dc, buf, -1, &r3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 40;

    // Пилюли — типы практик
    SelectObject(dc, gF.seg10);
    SetTextColor(dc, CLR_MUTED);
    RECT r4 = { UI_PAD, y, WIN_W - UI_PAD, y + 22 };
    DrawTextW(dc, L"ТИПЫ ПРАКТИК", -1, &r4, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 30;

    // Отрисовка пилюль (горизонтально, переносом)
    int px = UI_PAD;
    int py = y;
    int pillH = 32;
    SelectObject(dc, gF.seg10);
    for (int i = 0; i < PRACTICE_TYPES_COUNT; i++) {
        SIZE sz;
        GetTextExtentPoint32W(dc, PRACTICE_TYPES[i], (int)wcslen(PRACTICE_TYPES[i]), &sz);
        int pillW = sz.cx + 24;
        if (px + pillW > WIN_W - UI_PAD) {
            px = UI_PAD; py += pillH + 6;
        }
        RECT rp = { px, py, px + pillW, py + pillH };
        bool hov = (g_hover.kind == H_HAB_ROW && g_hover.idx == 1000 + i);
        // Фон пилюли
        gFillRound(g, (float)rp.left, (float)rp.top, (float)pillW, (float)pillH, 16,
                   hov ? CLR_SURFACE_HOV : CLR_SURFACE);
        // Текст пилюли
        SetTextColor(dc, hov ? CLR_ACCENT : CLR_TEXT);
        DrawTextW(dc, PRACTICE_TYPES[i], -1, &rp,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        px += pillW + 6;
    }
    y = py + pillH + 24;
    g.Flush(Gdiplus::FlushIntentionSync);

    // Кнопка "+ добавить практику"
    SelectObject(dc, gF.seg11);
    bool hoverAdd = (g_hover.kind == H_HAB_ADDBTN);
    SetTextColor(dc, hoverAdd ? CLR_ACCENT : CLR_TEXT);
    RECT rAdd = { UI_PAD, y, WIN_W - UI_PAD, y + 30 };
    DrawTextW(dc, L"+ добавить практику", -1, &rAdd,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 44;

    // Список записей за сегодня
    wchar_t today[11];
    getCurrentDate(today);
    SelectObject(dc, gF.seg9);
    SetTextColor(dc, CLR_MUTED);
    RECT rL = { UI_PAD, y, WIN_W - UI_PAD, y + 22 };
    DrawTextW(dc, L"СЕГОДНЯШНИЕ ЗАПИСИ", -1, &rL,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 28;

    SelectObject(dc, gF.seg11);
    SetTextColor(dc, CLR_TEXT);
    int shown = 0;
    for (int i = practiceCount - 1; i >= 0 && shown < 5; i--) {
        if (wcscmp(practices[i].date, today) != 0) continue;
        swprintf(buf, 64, L"%ls — %d мин", practices[i].type, practices[i].minutes);
        RECT re = { UI_PAD, y, WIN_W - UI_PAD - 30, y + 28 };
        DrawTextW(dc, buf, -1, &re, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        // × для удаления
        SetTextColor(dc, CLR_MUTED);
        RECT rx = { WIN_W - UI_PAD - 20, y, WIN_W - UI_PAD, y + 28 };
        DrawTextW(dc, L"×", -1, &rx, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, CLR_TEXT);
        y += 30;
        shown++;
    }
    if (shown == 0) {
        SelectObject(dc, gF.seg10);
        SetTextColor(dc, CLR_MUTED);
        RECT re = { UI_PAD, y, WIN_W - UI_PAD, y + 28 };
        DrawTextW(dc, L"сегодня ещё не было практики", -1, &re,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += 30;
    }
    y += 20;

    // Мини-календарь на месяц
    SelectObject(dc, gF.seg9);
    SetTextColor(dc, CLR_MUTED);
    RECT rc1 = { UI_PAD, y, WIN_W - UI_PAD, y + 22 };
    DrawTextW(dc, L"КАЛЕНДАРЬ МЕСЯЦА", -1, &rc1,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 28;

    // Заголовки дней
    static const wchar_t *wdays[7] = { L"ПН", L"ВТ", L"СР", L"ЧТ", L"ПТ", L"СБ", L"ВС" };
    int cellW = (WIN_W - UI_PAD * 2) / 7;
    SelectObject(dc, gF.seg8);
    for (int i = 0; i < 7; i++) {
        SetTextColor(dc, CLR_MUTED);
        RECT rd = { UI_PAD + i * cellW, y, UI_PAD + (i + 1) * cellW, y + 18 };
        DrawTextW(dc, wdays[i], -1, &rd, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    y += 22;

    // Сетка точек — 31 день
    time_t t = time(NULL);
    struct tm *ti = localtime(&t);
    int month = ti->tm_mon + 1;
    int year = ti->tm_year + 1900;
    int daysInMonth = 31;
    if (month == 2) daysInMonth = 28;
    else if (month == 4 || month == 6 || month == 9 || month == 11) daysInMonth = 30;

    for (int day = 1; day <= daysInMonth; day++) {
        int col = (day - 1) % 7;
        int row = (day - 1) / 7;
        float cx = (float)(UI_PAD + col * cellW + cellW / 2);
        float cy = (float)(y + row * 20 + 8);

        // Проверяем, была ли практика в этот день
        wchar_t dstr[11];
        swprintf(dstr, 11, L"%02d.%02d.%04d", day, month, year);
        bool done = false;
        for (int i = 0; i < practiceCount; i++) {
            if (wcscmp(practices[i].date, dstr) == 0) { done = true; break; }
        }

        if (done) {
            // Заполненная точка
            Gdiplus::SolidBrush br(gc(CLR_ACCENT));
            g.FillEllipse(&br, cx - 3, cy - 3, 6.0f, 6.0f);
        } else {
            // Контур
            Gdiplus::Pen p(gc(CLR_DIVIDER), 1.0f);
            g.DrawEllipse(&p, cx - 3, cy - 3, 6.0f, 6.0f);
        }
        g.Flush(Gdiplus::FlushIntentionSync);
    }
}

static void paintBody(HDC dc, Gdiplus::Graphics &g) {
    Layout L = getLayout();
    int vh = viewBodyH();

    g.SetClip(Gdiplus::Rect(0, UI_BODY_TOP, WIN_W, vh));
    int saved = SaveDC(dc);
    IntersectClipRect(dc, 0, UI_BODY_TOP, WIN_W, g_viewH);

    switch (g_activeTab) {
        case TAB_TODAY:     paintToday(dc, g, L);       break;
        case TAB_HABITS:    paintHabitsTab(dc, g, L);   break;
        case TAB_TASKS:     paintTasksTab(dc, g, L);    break;
        case TAB_LANGUAGE:  paintLanguageTab(dc, g, L); break;
        case TAB_PRACTICE:  paintPracticeTab(dc, g, L); break;
    }

    int ms = maxScroll();
    if (ms > 0) {
        float th = (float)vh * vh / L.contentH;
        if (th < 24.0f) th = 24.0f;
        float ty = UI_BODY_TOP + (vh - th) * g_scroll / ms;
        gFillRound(g, (float)(WIN_W - 12), ty, 3.0f, th, 1.5f, CLR_DIVIDER);
    }
    g.Flush(Gdiplus::FlushIntentionSync);

    RestoreDC(dc, saved);
    g.ResetClip();
}

// ===== Рамка окна =====
static void applyMainWindowChrome(HWND hWnd) {
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;
    typedef HRESULT (WINAPI *SetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
    SetAttrFn fn = (SetAttrFn)GetProcAddress(hDwm, "DwmSetWindowAttribute");
    if (fn) {
        BOOL dark = TRUE;           fn(hWnd, 20, &dark, sizeof(dark));
        COLORREF brd = CLR_DIVIDER; fn(hWnd, 34, &brd,  sizeof(brd));
        COLORREF cap = CLR_BG;      fn(hWnd, 35, &cap,  sizeof(cap));
        COLORREF txt = CLR_TEXT;    fn(hWnd, 36, &txt,  sizeof(txt));
    }
    FreeLibrary(hDwm);
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            gF.disp36 = makeFont(g_displayFace, 36, FALSE, g_displayWeight);
            gF.disp20 = makeFont(g_displayFace, 20, FALSE, g_displayWeight);
            gF.disp16 = makeFont(g_displayFace, 16, FALSE, g_displayWeight);
            gF.seg8   = makeFont(L"Segoe UI",  8, FALSE, FW_NORMAL);
            gF.seg9   = makeFont(L"Segoe UI",  9, FALSE, FW_NORMAL);
            gF.seg10  = makeFont(L"Segoe UI", 10, FALSE, FW_NORMAL);
            gF.seg11  = makeFont(L"Segoe UI", 11, FALSE, FW_NORMAL);
            gF.seg11i = makeFont(L"Segoe UI", 11, TRUE,  FW_NORMAL);
            gF.seg11s = makeFontStrike(L"Segoe UI", 11);
            gF.btn    = makeFont(L"Segoe UI", 10, FALSE, FW_SEMIBOLD);

            SetTimer(hWnd, ID_TIMER_CARET, 500, NULL);

            loadFromFile();
            loadHabits();
            loadPractices();
            loadLanguages();
            loadWords();
            refreshList();
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, WIN_W, g_viewH);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
            HGDIOBJ oldFont = GetCurrentObject(memDC, OBJ_FONT);

            fillRectColor(memDC, 0, 0, WIN_W, g_viewH, CLR_BG);
            SetBkMode(memDC, TRANSPARENT);
            {
                Gdiplus::Graphics g(memDC);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                paintBody(memDC, g);
                paintHeader(memDC, g);
                g.Flush(Gdiplus::FlushIntentionSync);
            }

            BitBlt(hdc, 0, 0, WIN_W, g_viewH, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldFont);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);
                Hit h = hitTest(pt.x, pt.y);
                LPCWSTR c = IDC_HAND;
                if (h.kind == H_NONE) c = IDC_ARROW;
                else if (h.kind == H_INPUT || h.kind == H_HAB_INPUT) c = IDC_IBEAM;
                else if (h.kind == H_HAB_ROW) c = IDC_ARROW;
                SetCursor(LoadCursor(NULL, c));
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
            g_hover.kind = H_NONE; g_hover.idx = -1; g_hover.idx2 = -1;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONDOWN: {
            SetFocus(hWnd);
            SetActiveWindow(hWnd);
            Hit h = hitTest((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
            switch (h.kind) {
                case H_ADD:   g_focus = FOCUS_TASK; addZapis(hWnd);    break;
                case H_DEL:   deleteZapis(hWnd); break;
                case H_HIDE:  ShowWindow(hWnd, SW_HIDE); break;
                case H_PRIO:  g_newPrio = h.idx; break;
                case H_INPUT: g_focus = FOCUS_TASK; g_caretOn = true; break;
                case H_CHECK: toggleDone(h.idx); break;
                case H_CARD:  g_selected = h.idx; break;
                case H_HAB_CHECK:
                    habits[h.idx].done[h.idx2] = !habits[h.idx].done[h.idx2];
                    saveHabits();
                    break;
                case H_HAB_DEL:   deleteHabit(hWnd, h.idx); break;
                case H_HAB_ROW:   break;
                case H_HAB_ADDBTN: {
                    g_habAdding = true;
                    g_focus = FOCUS_HABIT;
                    g_hInputLen = 0; g_hInput[0] = 0;
                    g_caretOn = true;
                    Layout L = getLayout();
                    ensureVisible(L.habAdd, 36 + 16);
                    break;
                }
                case H_HAB_INPUT: g_focus = FOCUS_HABIT; g_caretOn = true; break;
                case H_HAB_OK:    commitHabit(hWnd); break;
                case H_TAB:
                    g_activeTab = h.idx;
                    g_scroll = 0;
                    break;
                case H_TODAY_ADD_TASK:
                    g_activeTab = TAB_TASKS;
                    g_scroll = 0;
                    break;
                case H_TODAY_ADD_PRACTICE:
                    g_activeTab = TAB_PRACTICE;
                    g_scroll = 0;
                    break;
                case H_TODAY_ADD_WORD:
                    g_activeTab = TAB_LANGUAGE;
                    g_scroll = 0;
                    break;
                case H_CALENDAR_ICON:
                    MessageBoxW(hWnd, L"Календарь скоро!", L"Focus Bitch", MB_OK | MB_ICONINFORMATION);
                    break;
                case H_LF_ROW:
                    if (h.idx >= 0 && h.idx < LANG_TEMPLATES_COUNT)
                        g_langSelected[h.idx] = !g_langSelected[h.idx];
                    break;
                case H_LF_OK:
                    commitLangForm();
                    break;
                case H_LF_CANCEL:
                    cancelLangForm();
                    break;
                case H_LANG_ADD:
                    g_langForm = true;
                    g_openLang = -1;
                    for (int i = 0; i < LANG_TEMPLATES_COUNT; i++) g_langSelected[i] = false;
                    break;
                case H_LANG_DEL:
                    if (h.idx >= 0 && h.idx < languageCount) {
                        for (int i = h.idx; i < languageCount - 1; i++)
                            languages[i] = languages[i + 1];
                        languageCount--;
                        if (g_openLang >= languageCount) g_openLang = -1;
                        saveLanguages();
                    }
                    break;
                case H_LANG_OPEN:
                    if (h.idx >= 0 && h.idx < languageCount) {
                        g_openLang = h.idx;
                        g_scroll = 0;
                    }
                    break;
                case H_LANG_BACK:
                    g_openLang = -1;
                    g_scroll = 0;
                    break;
                case H_DAY_PILL: {
                    int pillDay = 0;
                    getPillDate(h.idx, NULL, &pillDay);
                    g_viewDay = pillDay;
                    g_scroll = 0;
                    break;
                }
                case H_DAY_PREV: {
                    int vd = viewDayNum();
                    g_viewDay = vd - 7;
                    g_scroll = 0;
                    break;
                }
                case H_DAY_NEXT: {
                    int vd = viewDayNum();
                    g_viewDay = vd + 7;
                    g_scroll = 0;
                    break;
                }
                case H_PF_TYPE:
                    g_practiceField = 0;
                    break;
                case H_PF_MIN:
                    g_practiceField = 1;
                    break;
                case H_PF_OK:
                    commitPracticeForm();
                    break;
                case H_PF_CANCEL:
                    cancelPracticeForm();
                    break;
                case H_PF_PILL:
                    if (h.idx >= 0 && h.idx < PRACTICE_TYPES_COUNT) {
                        wcsncpy(g_practiceTypeInput, PRACTICE_TYPES[h.idx], PRACT_NAME_MAX - 1);
                        g_practiceTypeInput[PRACT_NAME_MAX - 1] = 0;
                        g_practiceTypeLen = (int)wcslen(g_practiceTypeInput);
                        g_practiceField = 1;  // переход к минутам
                        if (g_practiceMinLen == 0) {
                            wcscpy(g_practiceMinInput, L"30");
                            g_practiceMinLen = 2;
                        }
                    }
                    break;
                case H_PRACTICE_ADD:
                    // Открыть форму добавления
                    g_practiceForm = true;
                    g_practiceField = 0;
                    g_practiceTypeInput[0] = 0; g_practiceTypeLen = 0;
                    g_practiceMinInput[0] = 0; g_practiceMinLen = 0;
                    g_caretOn = true;
                    break;
                case H_PRACTICE_PILL:
                    // Клик на пилюле на странице PRACTICE (не в форме) —
                    // открываем форму с предзаполненным типом
                    g_practiceForm = true;
                    g_practiceField = 1;
                    if (h.idx >= 0 && h.idx < PRACTICE_TYPES_COUNT) {
                        wcsncpy(g_practiceTypeInput, PRACTICE_TYPES[h.idx], PRACT_NAME_MAX - 1);
                        g_practiceTypeInput[PRACT_NAME_MAX - 1] = 0;
                        g_practiceTypeLen = (int)wcslen(g_practiceTypeInput);
                    }
                    wcscpy(g_practiceMinInput, L"30");
                    g_practiceMinLen = 2;
                    g_caretOn = true;
                    break;
                case H_PRACTICE_DEL:
                    if (h.idx >= 0 && h.idx < practiceCount) {
                        for (int i = h.idx; i < practiceCount - 1; i++)
                            practices[i] = practices[i + 1];
                        practiceCount--;
                        savePractices();
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    break;
                default:          g_selected = -1; break;
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

            // Форма практики — ввод в поля
            if (g_practiceForm) {
                if (c >= 32 && c != 127) {
                    if (g_practiceField == 0 && g_practiceTypeLen < PRACT_NAME_MAX - 1) {
                        g_practiceTypeInput[g_practiceTypeLen++] = c;
                        g_practiceTypeInput[g_practiceTypeLen] = 0;
                    } else if (g_practiceField == 1 && g_practiceMinLen < 6 && c >= L'0' && c <= L'9') {
                        g_practiceMinInput[g_practiceMinLen++] = c;
                        g_practiceMinInput[g_practiceMinLen] = 0;
                    }
                    g_caretOn = true;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0;
            }

            TextBuf b = activeBuf();
            if (c >= 32 && c != 127 && *b.len < b.cap - 1) {
                b.s[(*b.len)++] = c;
                b.s[*b.len] = 0;
                g_caretOn = true;
                invalidateActiveInput(hWnd);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            // Форма практики
            if (g_practiceForm) {
                if (wParam == VK_ESCAPE) {
                    cancelPracticeForm();
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    commitPracticeForm();
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == VK_TAB) {
                    g_practiceField = (g_practiceField == 0) ? 1 : 0;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == VK_BACK) {
                    if (g_practiceField == 0 && g_practiceTypeLen > 0) {
                        g_practiceTypeLen--;
                        g_practiceTypeInput[g_practiceTypeLen] = 0;
                    } else if (g_practiceField == 1 && g_practiceMinLen > 0) {
                        g_practiceMinLen--;
                        g_practiceMinInput[g_practiceMinLen] = 0;
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                return 0;
            }

            TextBuf b = activeBuf();
            if (wParam == VK_BACK) {
                bufBackspace(b.s, b.len);
                g_caretOn = true;
                invalidateActiveInput(hWnd);
                return 0;
            }
            if (wParam == VK_RETURN) {
                if (g_focus == FOCUS_HABIT && g_habAdding) commitHabit(hWnd);
                else addZapis(hWnd);
                return 0;
            }
            if (wParam == VK_ESCAPE && g_habAdding) {
                cancelHabitAdd();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                pasteClipboard(hWnd, b.s, b.len, b.cap);
                g_caretOn = true;
                invalidateActiveInput(hWnd);
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
                if (todayDayNumber() != g_statDay) {
                    rolloverHabits();
                    updateStreak();
                    InvalidateRect(hWnd, NULL, FALSE);
                } else if (IsWindowVisible(hWnd)) {
                    invalidateActiveInput(hWnd);
                }
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
            saveHabits();
            Shell_NotifyIconW(NIM_DELETE, &nid);
            UnregisterHotKey(hWnd, ID_HOTKEY);
            {
                HFONT *fonts[] = { &gF.disp36, &gF.disp20, &gF.disp16, &gF.seg8, &gF.seg9,
                                   &gF.seg10, &gF.seg11, &gF.seg11i, &gF.seg11s, &gF.btn };
                for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
                    if (*fonts[i]) { DeleteObject(*fonts[i]); *fonts[i] = NULL; }
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR pCmdLine, int nCmdShow) {
    hInst = hInstance;

    // Данные всегда рядом с .exe (важно для автозапуска)
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        wchar_t *p = wcsrchr(exePath, L'\\');
        if (p) { *p = 0; SetCurrentDirectoryW(exePath); }
    }

    loadAppFonts();

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
    int frameH = (wr.bottom - wr.top) - WIN_H;

    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int fit = (wa.bottom - wa.top) - frameH - 24;
    g_viewH = WIN_H;
    if (fit < g_viewH) g_viewH = fit;
    if (g_viewH < 560) g_viewH = 560;

    wr.left = 0; wr.top = 0; wr.right = WIN_W; wr.bottom = g_viewH;
    AdjustWindowRect(&wr, style, FALSE);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    int wx = wa.left + ((wa.right - wa.left) - ww) / 2;
    int wy = wa.top  + ((wa.bottom - wa.top) - wh) / 2;

    hMainWnd = CreateWindowExW(0, L"DiaryMainWnd", L"Ежедневник", style,
        wx, wy, ww, wh, NULL, NULL, hInstance, NULL);

    if (!hMainWnd) {
        unloadAppFonts();
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
    unloadAppFonts();

    return (int)msg.wParam;
}
