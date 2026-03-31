// ==WindhawkMod==
// @id              text-expansion-everywhere
// @name            Text Expansion (Per-Process)
// @description     Injects into all processes locally to replace typed hotstrings. Supports multi-line blocks.
// @version         0.2
// @author          Wouter
// @include         *
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModSettings==
// - hotstrings:
//   - $name: Hotstrings Configuration
//   - $description: Define hotstrings here. Put the trigger in brackets (e.g., [btw]), followed by the replacement text on the next lines.
//   - $default: "[btw]\nby the way\n\n[brb]\nbe right back\n\n[sql]\nSELECT *\nFROM Users\nWHERE Id = 1;"
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
            // Normalize CRLF to LF
            if (!line.empty() && line.back() == L'\r') {
                line.pop_back();
            }
            
            // Check if the line denotes a new [trigger]
            if (line.length() >= 2 && line.front() == L'[' && line.back() == L']') {
                if (!currentTrigger.empty()) {
                    // Strip the trailing newline from the accumulated replacement block
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
                // Append multi-line content
                currentReplacement += line + L"\n";
            }
        }
        
        // Save the final trigger block
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
        if (c == L'\r') continue; // Ignore standalone carriage returns
        
        // Translate \n to an actual Enter key press
        if (c == L'\n') {
            INPUT ip = {0};
            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = VK_RETURN;
            inputs.push_back(ip);
            
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(ip);
            continue;
        }
        
        // Standard Unicode text injection
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
                
                SendBackspace((int)trigger.length() - 1);
                SendString(pair.second);
                
                t_isProcessing = false;
                ReleaseSRWLockShared(&g_lock);
                return true; 
            }
        }
        ReleaseSRWLockShared(&g_lock);
    }
    return false;
}

// Intercepts messages directly inside the UI loop of the target process
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

void Wh_ModUninit() {
    // Unhooking handled implicitly by Windhawk
}