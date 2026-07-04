// hotkey.cpp  Independent global hotkey module implementation
#include "hotkey.h"
#include <cstdio>
#include <cstring>

// ---- Helper: VK code <-> key name ----
struct KeyName { UINT vk; const char* name; };
static const KeyName KEY_NAMES[] = {
    {'A',"A"},{'B',"B"},{'C',"C"},{'D',"D"},{'E',"E"},{'F',"F"},
    {'G',"G"},{'H',"H"},{'I',"I"},{'J',"J"},{'K',"K"},{'L',"L"},
    {'M',"M"},{'N',"N"},{'O',"O"},{'P',"P"},{'Q',"Q"},{'R',"R"},
    {'S',"S"},{'T',"T"},{'U',"U"},{'V',"V"},{'W',"W"},{'X',"X"},
    {'Y',"Y"},{'Z',"Z"},
    {VK_F1,"F1"},{VK_F2,"F2"},{VK_F3,"F3"},{VK_F4,"F4"},
    {VK_F5,"F5"},{VK_F6,"F6"},{VK_F7,"F7"},{VK_F8,"F8"},
    {VK_F9,"F9"},{VK_F10,"F10"},{VK_F11,"F11"},{VK_F12,"F12"},
    {VK_ESCAPE,"Esc"},{VK_SPACE,"Space"},{VK_RETURN,"Enter"},
    {VK_TAB,"Tab"},{VK_BACK,"Backspace"},{VK_DELETE,"Delete"},
    {VK_INSERT,"Insert"},{VK_HOME,"Home"},{VK_END,"End"},
    {VK_PRIOR,"PgUp"},{VK_NEXT,"PgDn"},
    {VK_LEFT,"Left"},{VK_RIGHT,"Right"},{VK_UP,"Up"},{VK_DOWN,"Down"},
    {VK_PAUSE,"Pause"},{VK_SCROLL,"ScrollLock"},{VK_CAPITAL,"CapsLock"},
    {'0',"0"},{'1',"1"},{'2',"2"},{'3',"3"},{'4',"4"},
    {'5',"5"},{'6',"6"},{'7',"7"},{'8',"8"},{'9',"9"},
    {0, nullptr}
};

static const char* VKToName(UINT vk) {
    for (const KeyName* k = KEY_NAMES; k->name; k++)
        if (k->vk == vk) return k->name;
    return nullptr;
}

static UINT NameToVK(const char* name) {
    for (const KeyName* k = KEY_NAMES; k->name; k++)
        if (_stricmp(k->name, name) == 0) return k->vk;
    // Single char fallback
    if (name[0] && !name[1]) {
        char c = (char)toupper((unsigned char)name[0]);
        if (c >= 'A' && c <= 'Z') return (UINT)c;
        if (c >= '0' && c <= '9') return (UINT)c;
    }
    return 0;
}

// ---- HotkeyConfig methods ----

bool HotkeyConfig::Register(HWND hwnd, int hotkeyId) const {
    if (IsNone()) return false;
    return ::RegisterHotKey(hwnd, hotkeyId, modifiers, vk) != 0;
}

void HotkeyConfig::Unregister(HWND hwnd, int hotkeyId) const {
    ::UnregisterHotKey(hwnd, hotkeyId);
}

void HotkeyConfig::Format(char* buf, int bufSize) const {
    if (IsNone()) { snprintf(buf, bufSize, "(none)"); return; }
    char parts[128] = "";
    int off = 0;
    if (modifiers & MOD_CONTROL) { off += snprintf(parts+off, sizeof(parts)-off, "Ctrl+"); }
    if (modifiers & MOD_SHIFT)   { off += snprintf(parts+off, sizeof(parts)-off, "Shift+"); }
    if (modifiers & MOD_ALT)     { off += snprintf(parts+off, sizeof(parts)-off, "Alt+"); }
    if (modifiers & MOD_WIN)     { off += snprintf(parts+off, sizeof(parts)-off, "Win+"); }
    const char* keyName = VKToName(vk);
    if (keyName)
        snprintf(parts+off, sizeof(parts)-off, "%s", keyName);
    else
        snprintf(parts+off, sizeof(parts)-off, "VK_%u", vk);
    snprintf(buf, bufSize, "%s", parts);
}

// Serialize: "CS:Q" means Ctrl+Shift+Q, "A:F5" means Alt+F5, ":Q" means no modifier+Q
void HotkeyConfig::ToString(char* buf, int bufSize) const {
    if (IsNone()) { snprintf(buf, bufSize, "NONE"); return; }
    char mods[8] = "";
    int off = 0;
    if (modifiers & MOD_CONTROL) mods[off++] = 'C';
    if (modifiers & MOD_SHIFT)   mods[off++] = 'S';
    if (modifiers & MOD_ALT)     mods[off++] = 'A';
    if (modifiers & MOD_WIN)     mods[off++] = 'W';
    mods[off] = 0;
    const char* keyName = VKToName(vk);
    if (keyName)
        snprintf(buf, bufSize, "%s:%s", mods, keyName);
    else
        snprintf(buf, bufSize, "%s:VK_%u", mods, vk);
}

bool HotkeyConfig::FromString(const char* str) {
    if (!str) return false;
    if (_stricmp(str, "NONE") == 0) { modifiers = 0; vk = 0; return true; }
    const char* colon = strchr(str, ':');
    if (!colon) return false;
    // Parse modifiers
    UINT mods = 0;
    for (const char* p = str; p < colon; p++) {
        switch (*p) {
            case 'C': case 'c': mods |= MOD_CONTROL; break;
            case 'S': case 's': mods |= MOD_SHIFT;   break;
            case 'A': case 'a': mods |= MOD_ALT;     break;
            case 'W': case 'w': mods |= MOD_WIN;     break;
        }
    }
    // Parse key
    char keyBuf[32] = "";
    strncpy(keyBuf, colon + 1, sizeof(keyBuf) - 1);
    UINT vkCode = NameToVK(keyBuf);
    if (vkCode == 0 && _strnicmp(keyBuf, "VK_", 3) == 0) {
        vkCode = (UINT)atoi(keyBuf + 3);
    }
    if (vkCode == 0) return false;
    modifiers = mods;
    vk = vkCode;
    return true;
}
