// ==WindhawkMod==
// @id              text-expansion-everywhere
// @name            Text Expansion (Per-Process)
// @description     Injects into all processes locally to replace typed hotstrings
// @version         0.1
// @author          Wouter
// @include         *
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModSettings==
// - hotstrings:
//   - $name: Hotstrings Configuration
//   - $description: Format: hotstring=replacement (comma-separated)
//   - $default: btw=by the way, brb=be right back
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

std::unordered_map<std::wstring, std::wstring> g_expansions;
size_t g_maxHotstringLength = 0;
SRWLOCK g_lock = SRWLOCK_INIT;

// Use thread_local because this DLL runs in all threads of all processes
thread_local std::wstring t_buffer;
thread_local bool t_isProcessing = false;

void LoadSettings() {
    PCWSTR settingsStr = Wh_GetStringSetting(L"hotstrings");
    if (settingsStr) {
        std::wstring s(settingsStr);
        Wh_FreeStringSetting(settingsStr);
        
        AcquireSRWLockExclusive(&g_lock);
        g_expansions.clear();
        g_maxHotstringLength = 0;
        
        std::wstringstream ss(s);
        std::wstring item;
        
        while (std::getline(ss, item, L',')) {
            size_t start = item.find_first_not_of(L" \t");
            if (start != std::wstring::npos) item = item.substr(start);

            size_t eqPos = item.find(L'=');
            if (eqPos != std::wstring::npos) {
                std::wstring trigger = item.substr(0, eqPos);
                std::wstring replacement = item.substr(eqPos + 1);
                
                if (!trigger.empty()) {
                    g_expansions[trigger] = replacement;
                    if (trigger.length() > g_maxHotstringLength) {
                        g_maxHotstringLength = trigger.length();
                    }
                }
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
                return true; // Match found, swallow the final trigger key
            }
        }
        ReleaseSRWLockShared(&g_lock);
    }
    return false;
}

// Hook GetMessageW
typedef BOOL (WINAPI *GetMessageW_t)(LPMSG, HWND, UINT, UINT);
GetMessageW_t GetMessageW_Original;

BOOL WINAPI GetMessageW_Hook(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
    BOOL res = GetMessageW_Original(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
    if (res > 0 && lpMsg) {
        if (lpMsg->message == WM_KEYDOWN || lpMsg->message == WM_SYSKEYDOWN) {
            if (ProcessKey((DWORD)lpMsg->wParam)) {
                lpMsg->message = WM_NULL; // Swallow the triggering keystroke
            }
        }
    }
    return res;
}

// Hook PeekMessageW
typedef BOOL (WINAPI *PeekMessageW_t)(LPMSG, HWND, UINT, UINT, UINT);
PeekMessageW_t PeekMessageW_Original;

BOOL WINAPI PeekMessageW_Hook(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
    BOOL res = PeekMessageW_Original(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    if (res && lpMsg && (wRemoveMsg & PM_REMOVE)) {
        if (lpMsg->message == WM_KEYDOWN || lpMsg->message == WM_SYSKEYDOWN) {
            if (ProcessKey((DWORD)lpMsg->wParam)) {
                lpMsg->message = WM_NULL; // Swallow the triggering keystroke
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
    // Windhawk automatically handles API unhooking on unload
}