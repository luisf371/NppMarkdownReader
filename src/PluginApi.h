#pragma once

#include <windows.h>
#include <commctrl.h>
#include <cstdint>

// ABI-compatible declarations required by this plugin. SCNotification is a
// prefix-compatible declaration containing every field this plugin reads;
// Notepad++ owns the notification storage passed to beNotified().
using Sci_Position = std::intptr_t;
using uptr_t = std::uintptr_t;
using sptr_t = std::intptr_t;

struct Sci_NotifyHeader {
    void* hwndFrom = nullptr;
    uptr_t idFrom = 0;
    unsigned int code = 0;
};

struct SCNotification {
    Sci_NotifyHeader nmhdr;
    Sci_Position position = 0;
    int ch = 0;
    int modifiers = 0;
    int modificationType = 0;
    const char* text = nullptr;
    Sci_Position length = 0;
    Sci_Position linesAdded = 0;
    int message = 0;
    uptr_t wParam = 0;
    sptr_t lParam = 0;
    Sci_Position line = 0;
    int foldLevelNow = 0;
    int foldLevelPrev = 0;
    int margin = 0;
    int listType = 0;
    int x = 0;
    int y = 0;
    int token = 0;
    Sci_Position annotationLinesAdded = 0;
    int updated = 0;
    int listCompletionMethod = 0;
    int characterSource = 0;
};

struct NppData {
    HWND _nppHandle = nullptr;
    HWND _scintillaMainHandle = nullptr;
    HWND _scintillaSecondHandle = nullptr;
};

using PFUNCPLUGINCMD = void(__cdecl*)();

struct ShortcutKey {
    bool _isCtrl = false;
    bool _isAlt = false;
    bool _isShift = false;
    UCHAR _key = 0;
};

inline constexpr int kPluginMenuItemSize = 64;
struct FuncItem {
    wchar_t _itemName[kPluginMenuItemSize]{};
    PFUNCPLUGINCMD _pFunc = nullptr;
    int _cmdID = 0;
    bool _init2Check = false;
    ShortcutKey* _pShKey = nullptr;
};

struct toolbarIconsWithDarkMode {
    HBITMAP hToolbarBmp = nullptr;
    HICON hToolbarIcon = nullptr;
    HICON hToolbarIconDarkMode = nullptr;
};

inline constexpr UINT NPPMSG = WM_USER + 1000;
inline constexpr UINT NPPM_GETCURRENTSCINTILLA = NPPMSG + 4;
inline constexpr UINT NPPM_GETCURRENTDOCINDEX = NPPMSG + 23;
inline constexpr UINT NPPM_ACTIVATEDOC = NPPMSG + 28;
inline constexpr UINT NPPM_SAVECURRENTFILE = NPPMSG + 38;
inline constexpr UINT NPPM_SAVEALLFILES = NPPMSG + 39;
inline constexpr UINT NPPM_SETMENUITEMCHECK = NPPMSG + 40;
inline constexpr UINT NPPM_GETPLUGINSCONFIGDIR = NPPMSG + 46;
inline constexpr UINT NPPM_MENUCOMMAND = NPPMSG + 48;
inline constexpr UINT NPPM_GETPOSFROMBUFFERID = NPPMSG + 57;
inline constexpr UINT NPPM_GETFULLPATHFROMBUFFERID = NPPMSG + 58;
inline constexpr UINT NPPM_GETBUFFERIDFROMPOS = NPPMSG + 59;
inline constexpr UINT NPPM_GETCURRENTBUFFERID = NPPMSG + 60;
inline constexpr UINT NPPM_DOOPEN = NPPMSG + 77;
inline constexpr UINT NPPM_GETPLUGINHOMEPATH = NPPMSG + 97;
inline constexpr UINT NPPM_ADDTOOLBARICON_FORDARKMODE = NPPMSG + 101;
inline constexpr UINT NPPM_ISDARKMODEENABLED = NPPMSG + 107;
inline constexpr UINT NPPM_ADDSCNMODIFIEDFLAGS = NPPMSG + 117;

inline constexpr int IDM_FILE = 41000;
inline constexpr int IDM_FILE_SAVEAS = IDM_FILE + 8;

inline constexpr int MAIN_VIEW = 0;
inline constexpr int SUB_VIEW = 1;

inline constexpr unsigned long SC_MOD_INSERTTEXT = 0x1;
inline constexpr unsigned long SC_MOD_DELETETEXT = 0x2;

inline constexpr UINT SCI_GETCURRENTPOS = 2008;
inline constexpr UINT SCI_GOTOPOS = 2025;
inline constexpr UINT SCI_GETCODEPAGE = 2137;
inline constexpr UINT SCI_GETTEXT = 2182;
inline constexpr UINT SCI_GETTEXTLENGTH = 2183;
inline constexpr UINT SCI_GETFIRSTVISIBLELINE = 2152;
inline constexpr UINT SCI_GETLINECOUNT = 2154;
inline constexpr UINT SCI_LINEFROMPOSITION = 2166;
inline constexpr UINT SCI_POSITIONFROMLINE = 2167;
inline constexpr UINT SCI_LINESONSCREEN = 2370;
inline constexpr UINT SCI_GRABFOCUS = 2400;
inline constexpr UINT SCI_SETFIRSTVISIBLELINE = 2613;
inline constexpr int SC_CP_UTF8 = 65001;

inline constexpr UINT SCN_UPDATEUI = 2007;
inline constexpr UINT SCN_MODIFIED = 2008;

inline constexpr UINT NPPN_FIRST = 1000;
inline constexpr UINT NPPN_READY = NPPN_FIRST + 1;
inline constexpr UINT NPPN_TBMODIFICATION = NPPN_FIRST + 2;
inline constexpr UINT NPPN_FILEBEFORECLOSE = NPPN_FIRST + 3;
inline constexpr UINT NPPN_FILEOPENED = NPPN_FIRST + 4;
inline constexpr UINT NPPN_FILECLOSED = NPPN_FIRST + 5;
inline constexpr UINT NPPN_FILESAVED = NPPN_FIRST + 8;
inline constexpr UINT NPPN_SHUTDOWN = NPPN_FIRST + 9;
inline constexpr UINT NPPN_BUFFERACTIVATED = NPPN_FIRST + 10;
inline constexpr UINT NPPN_LANGCHANGED = NPPN_FIRST + 11;
inline constexpr UINT NPPN_FILERENAMED = NPPN_FIRST + 23;
inline constexpr UINT NPPN_FILEDELETED = NPPN_FIRST + 26;
inline constexpr UINT NPPN_DARKMODECHANGED = NPPN_FIRST + 27;
inline constexpr UINT NPPN_GLOBALMODIFIED = NPPN_FIRST + 30;
inline constexpr UINT NPPN_TOOLBARICONSETCHANGED = NPPN_FIRST + 32;
