// ==WindhawkMod==
// @id              text-expansion-everywhere
// @name            Text Expansion (Per-Process)
// @description     Injects into all processes locally to replace typed hotstrings. Supports multi-line blocks and variables.
// @version         0.4
// @author          Wouter
// @include         *
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- hotstrings: |
    [btw]
    by the way

    [sql]
    SELECT *
    FROM Users
    WHERE Id = 1;

    [now]
    Current Date: %DATE%
    Current Time: %TIME%
  $name: Hotstrings Configuration
  $description: Define hotstrings here. Put the trigger in brackets. Supported variables: %DATE%, %TIME%.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

std::unordered_map<std::wstring, std::wstring> g_expansions;
size_t g_maxHotstringLength = 0;
SRWLOCK g_lock = SRWLOCK_INIT;

thread_local std::wstring t_buffer;
thread_local bool t_isProcessing = false;

// Helper function to replace all occurrences of a substring
void ReplaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

// Fetches the current system date
std::wstring GetCurrentDateStr() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buffer[256];
    GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"yyyy-MM-dd", buffer, 256);
    return std::wstring(buffer);
}

// Fetches the current system time in 24-hour format
std::wstring GetCurrentTimeStr() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buffer[256];
    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, L"HH:mm", buffer, 256);
    return std::wstring(buffer);
}

// Scans the replacement text and injects live variables
std::wstring ProcessVariables(std::wstring text) {
    if (text.find(L"%DATE%") != std::wstring::npos) {
        ReplaceAll(text, L"%DATE%", GetCurrentDateStr());
    }
    if (text.find(L"%TIME%") != std::wstring::npos) {
        ReplaceAll(text, L"%TIME%", GetCurrentTimeStr());
    }
    return text;
}

// Parses the INI-style block configuration
void LoadSettings() {
    PCWSTR settingsStr = Wh_GetStringSetting(L"hotstrings");
    if (settingsStr) {
        std::wstring s(settingsStr);
        Wh_FreeStringSetting(settingsStr);
        
        AcquireSRWLockExclusive(&g_lock);
        g_expansions.clear();
        g_maxHotstringLength = 0;
        
        std::wstringstream ss(s);
        std::wstring line;
        std::wstring currentTrigger = L"";
        std::wstring currentReplacement = L"";
        
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == L'\r') {
                line.pop_back();
            }
            
            if (line.length() >= 2 && line.front() == L'[' && line.back() == L']') {
                if (!currentTrigger.empty()) {
                    if (!currentReplacement.empty() && currentReplacement.back() == L'\n') {
                        currentReplacement.pop_back();
                    }
                    g_expansions[currentTrigger] = currentReplacement;
                    if (currentTrigger.length() > g_maxHotstringLength) {
                        g_maxHotstringLength = currentTrigger.length();
                    }
                }
                currentTrigger = line.substr(1, line.length() - 2);
                currentReplacement = L"";
            } else if (!currentTrigger.empty()) {
                currentReplacement += line + L"\n";
            }
        }
        
        if (!currentTrigger.empty()) {
            if (!currentReplacement.empty() && currentReplacement.back() == L'\n') {
                currentReplacement.pop_back();
            }
            g_expansions[currentTrigger] = currentReplacement;
            if (currentTrigger.length() > g_maxHotstringLength) {
                g_maxHotstringLength = currentTrigger.length();
            }
        }
        
        ReleaseSRWLockExclusive(&g_lock);
    }
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

bool ProcessKey(DWORD vkCode) {
    if (t_isProcessing) return false;
    
    wchar_t c = 0;
    if (vkCode >= 'A' && vkCode <= 'Z') {
        bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        c = (shift ^ caps) ? (wchar_t)vkCode : (wchar_t)(vkCode + 32);
    } else if (vkCode >= '0' && vkCode <= '9') {
        c = (wchar_t)vkCode;
    } else if (vkCode == VK_SPACE) {
        c = L' ';
    } else if (vkCode == VK_BACK) {
        if (!t_buffer.empty()) t_buffer.pop_back();
        return false;
    } else if (vkCode != VK_SHIFT && vkCode != VK_CAPITAL) {
        t_buffer.clear();
        return false;
    }
    
    if (c != 0) {
        t_buffer += c;
        
        AcquireSRWLockShared(&g_lock);
        if (t_buffer.length() > g_maxHotstringLength) {
            t_buffer.erase(0, t_buffer.length() - g_maxHotstringLength);
        }
        
        for (const auto& pair : g_expansions) {
            const std::wstring& trigger = pair.first;
            if (t_buffer.length() >= trigger.length() && 
                t_buffer.compare(t_buffer.length() - trigger.length(), trigger.length(), trigger) == 0) {
                
                t_isProcessing = true;
                t_buffer.clear();
                
                // Block the current key, erase the typed hotstring, then send the processed text
                SendBackspace((int)trigger.length() - 1);
                
                std::wstring finalOutput = ProcessVariables(pair.second);
                SendString(finalOutput);
                
                t_isProcessing = false;
                ReleaseSRWLockShared(&g_lock);
                return true; 
            }
        }
        ReleaseSRWLockShared(&g_lock);
    }
    return false;
}

// Hooks
typedef BOOL (WINAPI *GetMessageW_t)(LPMSG, HWND, UINT, UINT);
GetMessageW_t GetMessageW_Original;

BOOL WINAPI GetMessageW_Hook(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
    BOOL res = GetMessageW_Original(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
    if (res > 0 && lpMsg) {
        if (lpMsg->message == WM_KEYDOWN || lpMsg->message == WM_SYSKEYDOWN) {
            if (ProcessKey((DWORD)lpMsg->wParam)) {
                lpMsg->message = WM_NULL; 
            }
        }
    }
    return res;
}

typedef BOOL (WINAPI *PeekMessageW_t)(LPMSG, HWND, UINT, UINT, UINT);
PeekMessageW_t PeekMessageW_Original;

BOOL WINAPI PeekMessageW_Hook(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
    BOOL res = PeekMessageW_Original(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    if (res && lpMsg && (wRemoveMsg & PM_REMOVE)) {
        if (lpMsg->message == WM_KEYDOWN || lpMsg->message == WM_SYSKEYDOWN) {
            if (ProcessKey((DWORD)lpMsg->wParam)) {
                lpMsg->message = WM_NULL;
            }
        }
    }
    return res;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}

BOOL Wh_ModInit() {
    LoadSettings();
    Wh_SetFunctionHook((void*)GetMessageW, (void*)GetMessageW_Hook, (void**)&GetMessageW_Original);
    Wh_SetFunctionHook((void*)PeekMessageW, (void*)PeekMessageW_Hook, (void**)&PeekMessageW_Original);
    return TRUE;
}

void Wh_ModUninit() {}