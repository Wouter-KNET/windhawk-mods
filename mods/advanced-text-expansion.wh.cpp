// ==WindhawkMod==
// @id              text-expansion-global
// @name            Text Expansion
// @description     System-wide text expansion with activation symbols and an extensible settings UI.
// @version         0.6
// @author          Wouter
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- activation_symbol:
  - $name: Activation Symbol
  - $description: The prefix symbol required to trigger an expansion (e.g., / or !).
  - $default: "/"
- instant_expansion:
  - $name: Instant Expansion
  - $description: If enabled, ignore the activation symbol and expand immediately upon typing the hotstring.
  - $type: bool
  - $default: false
- expansions:
  - $name: Hotstring Mappings
  - $description: Add your hotstrings and replacements here. Use \n in the replacement text for line breaks.
  - $default:
    - trigger: btw
      replacement: by the way
    - trigger: sql
      replacement: "SELECT *\nFROM Users\nWHERE Id = 1;"
    - trigger: now
      replacement: "Current Date: %DATE%\nCurrent Time: %TIME%"
  - trigger:
    - $name: Hotstring Trigger
    - $description: What you type (do not include the activation symbol here)
  - replacement:
    - $name: Replacement Text
    - $description: The text to insert (use \n for multiline)
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>

HHOOK g_hHook = NULL;
HANDLE g_hThread = NULL;
DWORD g_threadId = 0;

std::unordered_map<std::wstring, std::wstring> g_expansions;
std::wstring g_buffer;
size_t g_maxHotstringLength = 0;
bool g_isProcessing = false;
SRWLOCK g_lock = SRWLOCK_INIT;

bool g_instantExpansion = false;
std::wstring g_activationSymbol = L"/";

void ReplaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

std::wstring GetCurrentDateStr() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buffer[256];
    GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"yyyy-MM-dd", buffer, 256);
    return std::wstring(buffer);
}

std::wstring GetCurrentTimeStr() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buffer[256];
    // Formatted to 24-hour notation
    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, L"HH:mm", buffer, 256);
    return std::wstring(buffer);
}

std::wstring ProcessVariables(std::wstring text) {
    if (text.find(L"%DATE%") != std::wstring::npos) {
        ReplaceAll(text, L"%DATE%", GetCurrentDateStr());
    }
    if (text.find(L"%TIME%") != std::wstring::npos) {
        ReplaceAll(text, L"%TIME%", GetCurrentTimeStr());
    }
    return text;
}

// Dynamically iterates through the Windhawk settings array
void LoadSettings() {
    AcquireSRWLockExclusive(&g_lock);
    g_expansions.clear();
    g_maxHotstringLength = 0;
    
    g_instantExpansion = Wh_GetIntSetting(L"instant_expansion");
    
    PCWSTR symStr = Wh_GetStringSetting(L"activation_symbol");
    if (symStr) {
        g_activationSymbol = symStr;
        Wh_FreeStringSetting(symStr);
    }

    for (int i = 0; ; i++) {
        std::wstring triggerKey = L"expansions[" + std::to_wstring(i) + L"].trigger";
        std::wstring replKey = L"expansions[" + std::to_wstring(i) + L"].replacement";
        
        PCWSTR trigger = Wh_GetStringSetting(triggerKey.c_str());
        if (!trigger) break; // End of the array
        
        PCWSTR repl = Wh_GetStringSetting(replKey.c_str());
        if (repl) {
            std::wstring t(trigger);
            std::wstring r(repl);
            
            // Translate literal "\n" strings from the UI into real newlines
            ReplaceAll(r, L"\\n", L"\n");
            
            if (!g_instantExpansion) {
                t = g_activationSymbol + t;
            }
            
            if (!t.empty()) {
                g_expansions[t] = r;
                if (t.length() > g_maxHotstringLength) {
                    g_maxHotstringLength = t.length();
                }
            }
            Wh_FreeStringSetting(repl);
        }
        Wh_FreeStringSetting(trigger);
    }
    
    ReleaseSRWLockExclusive(&g_lock);
}

void SendBackspace(int count) {
    std::vector<INPUT> inputs;
    for (int i = 0; i < count; ++i) {
        INPUT ip = {0};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = VK_BACK;
        inputs.push_back(ip);
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(ip);
    }
    if (!inputs.empty()) SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}

void SendString(const std::wstring& str) {
    std::vector<INPUT> inputs;
    for (wchar_t c : str) {
        if (c == L'\r') continue; 
        
        if (c == L'\n') {
            INPUT ip = {0};
            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = VK_RETURN;
            inputs.push_back(ip);
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(ip);
            continue;
        }
        
        INPUT ip = {0};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wScan = c;
        ip.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(ip);
        
        ip.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(ip);
    }
    if (!inputs.empty()) SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}

// Maps standard US layout symbols for the keystroke buffer
wchar_t GetCharFromVK(DWORD vkCode) {
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    
    if (vkCode >= 'A' && vkCode <= 'Z') return (shift ^ caps) ? (wchar_t)vkCode : (wchar_t)(vkCode + 32);
    if (vkCode >= '0' && vkCode <= '9') return shift ? L")!@#$%^&*("[vkCode - '0'] : (wchar_t)vkCode;
    
    switch(vkCode) {
        case VK_SPACE:     return L' ';
        case VK_OEM_MINUS: return shift ? L'_' : L'-';
        case VK_OEM_PLUS:  return shift ? L'+' : L'=';
        case VK_OEM_1:     return shift ? L':' : L';';
        case VK_OEM_2:     return shift ? L'?' : L'/';
        case VK_OEM_3:     return shift ? L'~' : L'`';
        case VK_OEM_4:     return shift ? L'{' : L'[';
        case VK_OEM_5:     return shift ? L'|' : L'\\';
        case VK_OEM_6:     return shift ? L'}' : L']';
        case VK_OEM_7:     return shift ? L'"' : L'\'';
        case VK_OEM_COMMA: return shift ? L'<' : L',';
        case VK_OEM_PERIOD:return shift ? L'>' : L'.';
    }
    return 0;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        if (g_isProcessing) return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyBoard->vkCode;
        
        if (vkCode == VK_BACK) {
            if (!g_buffer.empty()) g_buffer.pop_back();
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }
        
        wchar_t c = GetCharFromVK(vkCode);
        
        if (c != 0) {
            g_buffer += c;
            
            AcquireSRWLockShared(&g_lock);
            if (g_buffer.length() > g_maxHotstringLength) {
                g_buffer.erase(0, g_buffer.length() - g_maxHotstringLength);
            }
            
            for (const auto& pair : g_expansions) {
                const std::wstring& trigger = pair.first;
                if (g_buffer.length() >= trigger.length() && 
                    g_buffer.compare(g_buffer.length() - trigger.length(), trigger.length(), trigger) == 0) {
                    
                    g_isProcessing = true;
                    g_buffer.clear();
                    
                    SendBackspace((int)trigger.length() - 1);
                    std::wstring finalOutput = ProcessVariables(pair.second);
                    SendString(finalOutput);
                    
                    g_isProcessing = false;
                    ReleaseSRWLockShared(&g_lock);
                    return 1; 
                }
            }
            ReleaseSRWLockShared(&g_lock);
        } else if (vkCode != VK_SHIFT && vkCode != VK_CAPITAL && vkCode != VK_LWIN && vkCode != VK_RWIN && vkCode != VK_CONTROL && vkCode != VK_MENU) {
            // Unmapped control keys (like Arrow keys, Escape, Enter) clear the buffer so triggers don't wrap across lines
            g_buffer.clear(); 
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

DWORD WINAPI HookThread(LPVOID lpParam) {
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    }
    return 0;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}

BOOL Wh_ModInit() {
    LoadSettings();
    g_hThread = CreateThread(NULL, 0, HookThread, NULL, 0, &g_threadId);
    return TRUE;
}

void Wh_ModUninit() {
    if (g_threadId) {
        PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_hThread, 1000);
        CloseHandle(g_hThread);
    }
}