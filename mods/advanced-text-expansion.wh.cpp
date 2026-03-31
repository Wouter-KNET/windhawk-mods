// ==WindhawkMod==
// @id              text-expansion
// @name            Text Expansion
// @description     Replaces typed hotstrings with configured replacement texts globally
// @version         0.0
// @author          Wouter
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModSettings==
// - hotstrings:
//   - $name: Hotstrings Configuration
//   - $description: Format: hotstring=replacement (comma-separated for multiple. e.g., btw=by the way, brb=be right back)
//   - $default: btw=by the way, brb=be right back, addr=123 Main Street
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

// Reloads configuration from the Windhawk UI
void LoadSettings() {
    PCWSTR settingsStr = Wh_GetStringSetting(L"hotstrings");
    if (settingsStr) {
        g_expansions.clear();
        g_maxHotstringLength = 0;
        
        std::wstring s(settingsStr);
        Wh_FreeStringSetting(settingsStr);
        
        std::wstringstream ss(s);
        std::wstring item;
        
        while (std::getline(ss, item, L',')) {
            // Trim leading whitespace
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
    }
}

// Emits VK_BACK to erase the hotstring characters
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
    if (!inputs.empty()) {
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
}

// Injects the replacement string using the Unicode flag
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
    if (!inputs.empty()) {
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
}

// Low-level keyboard hook procedure
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        if (g_isProcessing) return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyBoard->vkCode;
        
        wchar_t c = 0;
        
        // Basic Virtual-Key mapping
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
        else {
            g_buffer.clear(); // Reset buffer on unmapped keys (like Enter, Esc, etc.)
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }
        
        if (c != 0) {
            g_buffer += c;
            if (g_buffer.length() > g_maxHotstringLength) {
                g_buffer.erase(0, g_buffer.length() - g_maxHotstringLength);
            }
            
            // Check for a hotstring match
            for (const auto& pair : g_expansions) {
                const std::wstring& trigger = pair.first;
                if (g_buffer.length() >= trigger.length() && 
                    g_buffer.compare(g_buffer.length() - trigger.length(), trigger.length(), trigger) == 0) {
                    
                    g_isProcessing = true;
                    g_buffer.clear();
                    
                    // Erase typed characters (-1 because we block the final keydown from registering)
                    SendBackspace((int)trigger.length() - 1);
                    SendString(pair.second);
                    
                    g_isProcessing = false;
                    return 1; // Block the current keystroke so it doesn't output
                }
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// Background thread loop to keep the hook alive
DWORD WINAPI HookThread(LPVOID lpParam) {
    LoadSettings();
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

// Triggered automatically by Windhawk when user edits settings
void Wh_ModSettingsChanged() {
    LoadSettings();
}

BOOL Wh_ModInit() {
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