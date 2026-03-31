// ==WindhawkMod==
// @id              text-expansion-global
// @name            Text Expansion
// @description     Uses a system-wide low-level hook to replace typed hotstrings anywhere. Supports multi-line blocks and variables.
// @version         0.5
// @author          Wouter
// @include         explorer.exe
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

HHOOK g_hHook = NULL;
HANDLE g_hThread = NULL;
DWORD g_threadId = 0;

std::unordered_map<std::wstring, std::wstring> g_expansions;
std::wstring g_buffer;
size_t g_maxHotstringLength = 0;
bool g_isProcessing = false;
SRWLOCK g_lock = SRWLOCK_INIT;

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
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            
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

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        if (g_isProcessing) return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyBoard->vkCode;
        
        wchar_t c = 0;
        
        if (vkCode >= 'A' && vkCode <= 'Z') {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            c = (shift ^ caps) ? (wchar_t)vkCode : (wchar_t)(vkCode + 32);
        } 
        else if (vkCode >= '0' && vkCode <= '9') c = (wchar_t)vkCode;
        else if (vkCode == VK_SPACE) c = L' ';
        else if (vkCode == VK_BACK) {
            if (!g_buffer.empty()) g_buffer.pop_back();
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        } 
        else if (vkCode != VK_SHIFT && vkCode != VK_CAPITAL && vkCode != VK_LWIN && vkCode != VK_RWIN) {
            g_buffer.clear(); 
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }
        
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