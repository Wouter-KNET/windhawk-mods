// ==WindhawkMod==
// @id              text-expansion-global
// @name            Text Expansion
// @description     System-wide text expansion with activation symbols and an extensible settings UI.
// @version         0.44
// @author          Wouter
// @include         explorer.exe
// @compilerOptions -luser32 -lole32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Text Expansion

A powerful, system-wide text expansion and macro engine for Windows. This mod allows you to define custom triggers that automatically expand into larger snippets of text, variables, or even complex keyboard macros.

## Base Functionality & Settings
* **Activation Symbol:** By default, triggers require a prefix (like `/`) to prevent accidental expansions while typing normally. For example, if your trigger is `btw`, you must type `/btw`.
* **Terminator Characters:** Standard triggers require you to type a terminator character (like Space, Enter, or a comma) to confirm the trigger and execute the expansion. This allows overlapping triggers (like `/time` and `/time_long`) to coexist. *Note: The typed terminator is automatically preserved and appended after the expanded text.*
* **Custom Terminators:** If a specific rule (like a regex) requires typing characters that are normally terminators (like a colon `:`), you can define a custom terminator string for that rule to override the global list.
* **Instant Expansion:** Each hotstring mapping can individually be set to "Instant". If enabled, the activation symbol and terminators are ignored, and the text expands the exact moment the trigger is typed.
* **Is Regex:** Treat your trigger as a Regular Expression, allowing for dynamic pattern matching and capture groups.

## Variables and Formatting
You can use special variables inside your replacement text. They will be evaluated dynamically upon expansion:
* `%DATE%` / `%TIME%`: Inserts the current date (yyyy-MM-dd) or time (HH:mm).
* `%DATE(format)%` / `%TIME(format)%`: Use standard C# / .NET custom format strings (e.g., `%DATE(dddd, d MMMM yyyy)%` or `%TIME(HH:mm:ss.fff K)%`). 
  * *Note on Dates/Times:* Because this interacts with native Win32 APIs, Date and Time are still evaluated separately. To get a full .NET format, you should combine them: `%DATE(yyyy-MM-dd)% %TIME(HH:mm:ss.fff zzz)%`.
  * *Polyfilled .NET tokens:* `f` to `fffffff` (true 100-nanosecond tick precision), `F` to `FFFFFFF` (fractional without trailing zeros), `z` to `zzz` (timezone offset), and `K` (timezone info) are fully supported.
  * *Escaping:* You can use the backslash (`\`) to escape characters so they are output exactly as-is (e.g., `%TIME(HH\hmm\m)%` outputs `14h30m`).
* `%CLIPBOARD%`: Inserts the current text content of your clipboard.
* `%USERNAME%`: Inserts your Windows username.
* `%GUID%`: Generates a new random GUID (default format `D` with hyphens).
* `%GUID(format)%`: Formats the GUID. Supported values: `N` (digits), `D` (hyphens), `B` (braces), `P` (parentheses), `X` (hexadecimal struct).

## Special Functions
* **HTML Entities:** Convert a single character symbol to its HTML entity.
    * `%html_hex(€)%` outputs the hexadecimal entity `&#x20AC;`
    * `%html_dec(€)%` outputs the decimal entity `&#8364;`
    * *Note: Reserved characters (`<`, `>`, `&`, `"`, `'`) are automatically converted to their named entities (`&lt;`, etc.).*
* **Nested Expansion:** `%expand(trigger)%`
    * Call another snippet from within a snippet. You must provide the *exact* string you would normally type (including the activation symbol if the target is not an instant trigger).
    * Example: `%expand(/sig)%`
    * *Failsafe: Recursion is limited to a depth of 20 to prevent infinite macro loops.*

## Keyboard Actions & Cursor Placement
Simulate physical keystrokes within your snippets to automate form filling or code scaffolding:
* **Navigation:** `%TAB%`, `%ENTER%`, `%ESC%`, `%BACKSPACE%`, `%HOME%`, `%END%`, `%PGDN%`, `%PGUP%`, `%DELETE%`
* **Media Controls:** `%PLAYPAUSE%`, `%PREV%`, `%NEXT%`, `%VOLUP%`, `%VOLDOWN%`
* **`%CURSOR%`**: Place this anywhere in your replacement text. After the text expands, the mod will automatically calculate the offset and use `Left Arrow` keystrokes to position your typing cursor exactly at this spot.

## Regular Expressions
When **Is Regex** is checked, your trigger acts as a regex pattern. You can use capture groups like `(.*)` and reference them in your replacement text using `$1`, `$2`, etc.
*Note: If the trigger is not instant, the activation symbol is automatically prepended to your pattern and safely escaped in the regex engine.*

The underlying regex engine uses the **ECMAScript** grammar, which is the default in standard C++ and highly compatible with .NET (PCRE-style) regular expressions.

## Examples
**1. HTML Wrapper (using Clipboard & Cursor)**
* **Trigger:** `a` (Instant)
* **Replacement:** `<a href="%CLIPBOARD%">%CURSOR%</a>`
* **Result:** Wraps your clipboard URL in an anchor tag and places your cursor ready to type the link text.

**2. C# Exception Logging (using Polyfilled Fractional Time)**
* **Trigger:** `catch` (Instant)
* **Replacement:** `catch (Exception ex)%ENTER%{%ENTER%%TAB%_logger.LogError(ex, "Error occurred at %TIME(HH:mm:ss.fffffff)%");%ENTER%}`

**3. Dynamic Regex Match with Custom Terminator**
* **Trigger:** `time-(.*)` (Is Regex: True, Activation Symbol: `/`, Custom Terminator: `\s\n`)
* **Replacement:** `The format requested was $1: %TIME($1)%`
* **Result:** Because `:` is omitted from the custom terminator, you can safely type `/time-HH:mm` and press Space to trigger the expansion.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- activation_symbol: "/"
  $name: Activation Symbol
  $description: The prefix symbol required to trigger an expansion (e.g. / or !).
- terminator_characters: "\\s,.\\n\\t?!;:"
  $name: Terminator Characters
  $description: Characters that confirm and trigger a non-instant expansion. Use \n for Enter, \t for Tab, \s for Space.
- expansions:
  - - trigger: "btw"
      $name: Hotstring Trigger
      $description: What you type (do not include the activation symbol here)
    - replacement: "by the way"
      $name: Replacement Text
      $description: The text to insert (use \n for multiline)
    - terminators: ""
      $name: Custom Terminators
      $description: (Optional) Override the global terminator characters for this specific trigger (e.g., \s\n). Leave empty to use global terminators.
    - instant: false
      $name: Instant Expansion
      $description: If true, ignore the activation symbol and terminators. Expands immediately upon typing the hotstring.
    - is_regex: false
      $name: Is Regex
      $description: If true, the trigger is treated as a Regular Expression. Capture groups like $1 can be used in the Replacement Text.
  $name: Hotstring Mappings
  $description: "Add your hotstrings here. Variables: %DATE%, %TIME%, %DATE(format)%, %TIME(format)%, %CLIPBOARD%, %USERNAME%, %GUID%, %GUID(format)% (N, D, B, P, X), %CURSOR%, %TAB%, %ENTER%, %ESC%, %BACKSPACE%, %HOME%, %END%, %PGDN%, %PGUP%, %DELETE%, %PLAYPAUSE%, %PREV%, %NEXT%, %VOLUP%, %VOLDOWN%, %html_dec(S)%, %html_hex(S)%, %expand(trigger)%."
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <cwctype>
#include <regex>
#include <objbase.h> // For CoCreateGuid

HHOOK g_hHook = NULL;
HANDLE g_hThread = NULL;
DWORD g_threadId = 0;

struct ExpansionRule {
    std::wstring triggerText;
    std::wregex triggerRegex;
    std::wstring replacementText;
    std::wstring terminators;
    bool isInstant;
    bool isRegex;
    bool isValidRegex;
};

std::vector<ExpansionRule> g_expansions;
std::wstring g_buffer;
size_t g_maxHotstringLength = 0;
SRWLOCK g_lock = SRWLOCK_INIT;

std::wstring g_activationSymbol = L"/";
std::wstring g_terminatorChars = L" ,.\n\t?!;:";

// Unified logging function for OutputDebugString and Windhawk Log
void DebugLog(const wchar_t* format, ...) {
    wchar_t buffer[2048];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, ARRAYSIZE(buffer), format, args);
    va_end(args);

    std::wstring sysLog = L"[TextExpansion] ";
    sysLog += buffer;
    sysLog += L"\n";
    OutputDebugStringW(sysLog.c_str());
    Wh_Log(L"%s", buffer);
}

void ReplaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

// Safely escapes regex metacharacters if the activation symbol is used before a regex
std::wstring EscapeRegex(const std::wstring& str) {
    std::wstring escaped;
    for (wchar_t c : str) {
        if (wcschr(L".^$*+?()[]{}|\\", c) != nullptr) {
            escaped += L'\\';
        }
        escaped += c;
    }
    return escaped;
}

// Polyfills dotnet-specific format tokens (like fff, FFF, zzz, K) and handles \ escaping
std::wstring PolyfillDotNetFormats(const std::wstring& format, ULONGLONG fractionalTicks) {
    TIME_ZONE_INFORMATION tzi;
    DWORD tzRes = GetTimeZoneInformation(&tzi);
    long totalBias = tzi.Bias;
    if (tzRes == TIME_ZONE_ID_DAYLIGHT) totalBias += tzi.DaylightBias;
    else if (tzRes == TIME_ZONE_ID_STANDARD) totalBias += tzi.StandardBias;
    
    int offsetMinutes = -totalBias;
    int hours = offsetMinutes / 60;
    int mins = abs(offsetMinutes % 60);

    std::wstring result;
    bool inQuotes = false;
    
    for (size_t i = 0; i < format.length(); ) {
        if (format[i] == L'\'') {
            inQuotes = !inQuotes;
            result += format[i];
            i++;
            continue;
        }

        if (!inQuotes) {
            // Handle backslash escaping
            if (format[i] == L'\\') {
                if (i + 1 < format.length()) {
                    wchar_t nextChar = format[i + 1];
                    if (nextChar == L'\'') {
                        result += L"''"; // Win32 literal single quote
                    } else {
                        result += L"'";
                        result += nextChar;
                        result += L"'";
                    }
                    i += 2;
                    continue;
                }
            }

            // Check f to fffffff (fractional seconds)
            if (format[i] == L'f') {
                int count = 0;
                while (i + count < format.length() && format[i + count] == L'f' && count < 7) count++;
                wchar_t buf[16];
                swprintf_s(buf, ARRAYSIZE(buf), L"%07llu", fractionalTicks);
                std::wstring val = std::wstring(buf).substr(0, count);
                result += L"'" + val + L"'"; // Wrap in quotes so Win32 formats treat it strictly as a literal
                i += count;
                continue;
            }
            // Check F to FFFFFFF (fractional seconds without trailing zeros)
            else if (format[i] == L'F') {
                int count = 0;
                while (i + count < format.length() && format[i + count] == L'F' && count < 7) count++;
                wchar_t buf[16];
                swprintf_s(buf, ARRAYSIZE(buf), L"%07llu", fractionalTicks);
                std::wstring val = std::wstring(buf).substr(0, count);
                while (val.length() > 0 && val.back() == L'0') val.pop_back();
                if (val.length() > 0) {
                    result += L"'" + val + L"'";
                }
                i += count;
                continue;
            }
            // Check z to zzz (Timezone Offset)
            else if (format[i] == L'z') {
                int count = 0;
                while (i + count < format.length() && format[i + count] == L'z' && count < 3) count++;
                wchar_t buf[16];
                if (count == 3) swprintf_s(buf, 16, L"%+03d:%02d", hours, mins);
                else if (count == 2) swprintf_s(buf, 16, L"%+03d", hours);
                else swprintf_s(buf, 16, L"%+d", hours);
                result += L"'" + std::wstring(buf) + L"'";
                i += count;
                continue;
            }
            // Check K (Timezone Information)
            else if (format[i] == L'K') {
                wchar_t buf[16];
                swprintf_s(buf, 16, L"%+03d:%02d", hours, mins);
                result += L"'" + std::wstring(buf) + L"'";
                i += 1;
                continue;
            }
        }
        result += format[i];
        i++;
    }
    return result;
}

std::wstring GetFormattedDate(const std::wstring& formatStr) {
    FILETIME ftUtc, ftLocal;
    GetSystemTimePreciseAsFileTime(&ftUtc);
    FileTimeToLocalFileTime(&ftUtc, &ftLocal);
    
    SYSTEMTIME stLocal;
    FileTimeToSystemTime(&ftLocal, &stLocal);
    
    ULARGE_INTEGER uli;
    uli.LowPart = ftLocal.dwLowDateTime;
    uli.HighPart = ftLocal.dwHighDateTime;
    ULONGLONG fractionalTicks = uli.QuadPart % 10000000;
    
    std::wstring format = PolyfillDotNetFormats(formatStr, fractionalTicks);
    
    wchar_t buffer[512]; // Increased buffer size to safely handle heavy escaping
    if (GetDateFormatW(LOCALE_USER_DEFAULT, 0, &stLocal, format.c_str(), buffer, ARRAYSIZE(buffer)) > 0) {
        return std::wstring(buffer);
    }
    return L""; 
}

std::wstring GetFormattedTime(const std::wstring& formatStr) {
    FILETIME ftUtc, ftLocal;
    GetSystemTimePreciseAsFileTime(&ftUtc);
    FileTimeToLocalFileTime(&ftUtc, &ftLocal);
    
    SYSTEMTIME stLocal;
    FileTimeToSystemTime(&ftLocal, &stLocal);
    
    ULARGE_INTEGER uli;
    uli.LowPart = ftLocal.dwLowDateTime;
    uli.HighPart = ftLocal.dwHighDateTime;
    ULONGLONG fractionalTicks = uli.QuadPart % 10000000;
    
    std::wstring format = PolyfillDotNetFormats(formatStr, fractionalTicks);

    wchar_t buffer[512];
    if (GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &stLocal, format.c_str(), buffer, ARRAYSIZE(buffer)) > 0) {
        return std::wstring(buffer);
    }
    return L"";
}

std::wstring GetClipboardText() {
    if (!OpenClipboard(nullptr)) return L"";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) { 
        CloseClipboard(); 
        return L""; 
    }
    wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
    if (!pszText) { 
        GlobalUnlock(hData);
        CloseClipboard(); 
        return L""; 
    }
    std::wstring text(pszText);
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
}

std::wstring GetUsernameStr() {
    wchar_t buffer[256];
    DWORD size = 256;
    if (GetUserNameW(buffer, &size)) {
        return std::wstring(buffer);
    }
    return L"";
}

std::wstring GetNewGuid(const std::wstring& format) {
    GUID guid;
    if (CoCreateGuid(&guid) != S_OK) return L"";
    
    wchar_t buffer[128]; // Increased to 128 to accommodate the X format
    // Mimic .NET standard GUID formatting (N, D, B, P, X)
    if (format == L"N" || format == L"n") {
        swprintf_s(buffer, ARRAYSIZE(buffer), L"%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    } else if (format == L"B" || format == L"b") {
        swprintf_s(buffer, ARRAYSIZE(buffer), L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    } else if (format == L"P" || format == L"p") {
        swprintf_s(buffer, ARRAYSIZE(buffer), L"(%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)",
            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    } else if (format == L"X" || format == L"x") {
        swprintf_s(buffer, ARRAYSIZE(buffer), L"{0x%08x,0x%04x,0x%04x,{0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x}}",
            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    } else { 
        // Default to D
        swprintf_s(buffer, ARRAYSIZE(buffer), L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    }
    return std::wstring(buffer);
}

std::wstring ProcessVariables(std::wstring text, int& outCursorOffset, int recursionDepth = 0) {
    if (recursionDepth > 20) {
        DebugLog(L"Expansion recursion limit reached. Aborting nested expansion to prevent infinite loops.");
        return text;
    }

    if (recursionDepth == 0) {
        outCursorOffset = 0;
    }

    if (text.find(L"%CLIPBOARD%") != std::wstring::npos) {
        ReplaceAll(text, L"%CLIPBOARD%", GetClipboardText());
    }
    if (text.find(L"%USERNAME%") != std::wstring::npos) {
        ReplaceAll(text, L"%USERNAME%", GetUsernameStr());
    }

    // Process Keyboard Actions (converted to special chars intercepted by SendString)
    // We use Private Use Area (PUA) characters (\xE000-\xE00A) to map advanced keystrokes safely
    if (text.find(L"%TAB%") != std::wstring::npos) ReplaceAll(text, L"%TAB%", L"\t");
    if (text.find(L"%ENTER%") != std::wstring::npos) ReplaceAll(text, L"%ENTER%", L"\n");
    if (text.find(L"%ESC%") != std::wstring::npos) ReplaceAll(text, L"%ESC%", L"\x1B");
    if (text.find(L"%BACKSPACE%") != std::wstring::npos) ReplaceAll(text, L"%BACKSPACE%", L"\xE000");
    if (text.find(L"%HOME%") != std::wstring::npos) ReplaceAll(text, L"%HOME%", L"\xE001");
    if (text.find(L"%END%") != std::wstring::npos) ReplaceAll(text, L"%END%", L"\xE002");
    if (text.find(L"%PGDN%") != std::wstring::npos) ReplaceAll(text, L"%PGDN%", L"\xE003");
    if (text.find(L"%PGUP%") != std::wstring::npos) ReplaceAll(text, L"%PGUP%", L"\xE004");
    if (text.find(L"%DELETE%") != std::wstring::npos) ReplaceAll(text, L"%DELETE%", L"\xE005");
    if (text.find(L"%PLAYPAUSE%") != std::wstring::npos) ReplaceAll(text, L"%PLAYPAUSE%", L"\xE006");
    if (text.find(L"%PREV%") != std::wstring::npos) ReplaceAll(text, L"%PREV%", L"\xE007");
    if (text.find(L"%NEXT%") != std::wstring::npos) ReplaceAll(text, L"%NEXT%", L"\xE008");
    if (text.find(L"%VOLUP%") != std::wstring::npos) ReplaceAll(text, L"%VOLUP%", L"\xE009");
    if (text.find(L"%VOLDOWN%") != std::wstring::npos) ReplaceAll(text, L"%VOLDOWN%", L"\xE00A");

    // Process standard/default replacements
    if (text.find(L"%DATE%") != std::wstring::npos) {
        ReplaceAll(text, L"%DATE%", GetFormattedDate(L"yyyy-MM-dd"));
    }
    if (text.find(L"%TIME%") != std::wstring::npos) {
        ReplaceAll(text, L"%TIME%", GetFormattedTime(L"HH:mm"));
    }
    if (text.find(L"%GUID%") != std::wstring::npos) {
        ReplaceAll(text, L"%GUID%", GetNewGuid(L"D"));
    }

    // Process custom format %DATE(format)%
    size_t pos = 0;
    while ((pos = text.find(L"%DATE(", pos)) != std::wstring::npos) {
        size_t endPos = text.find(L")%", pos + 6);
        if (endPos != std::wstring::npos) {
            std::wstring format = text.substr(pos + 6, endPos - pos - 6);
            std::wstring replacement = GetFormattedDate(format);
            text.replace(pos, endPos - pos + 2, replacement);
            pos += replacement.length();
        } else {
            pos += 6; // Malformed tag, just skip it
        }
    }

    // Process custom format %TIME(format)%
    pos = 0;
    while ((pos = text.find(L"%TIME(", pos)) != std::wstring::npos) {
        size_t endPos = text.find(L")%", pos + 6);
        if (endPos != std::wstring::npos) {
            std::wstring format = text.substr(pos + 6, endPos - pos - 6);
            std::wstring replacement = GetFormattedTime(format);
            text.replace(pos, endPos - pos + 2, replacement);
            pos += replacement.length();
        } else {
            pos += 6;
        }
    }

    // Process custom format %GUID(format)%
    pos = 0;
    while ((pos = text.find(L"%GUID(", pos)) != std::wstring::npos) {
        size_t endPos = text.find(L")%", pos + 6);
        if (endPos != std::wstring::npos) {
            std::wstring format = text.substr(pos + 6, endPos - pos - 6);
            std::wstring replacement = GetNewGuid(format);
            text.replace(pos, endPos - pos + 2, replacement);
            pos += replacement.length();
        } else {
            pos += 6;
        }
    }

    // Process custom function %html_dec(S)% and %html_hex(S)%
    auto processHtmlToken = [&](const std::wstring& tokenPrefix, bool useHex) {
        size_t tokenPos = 0;
        while ((tokenPos = text.find(tokenPrefix, tokenPos)) != std::wstring::npos) {
            size_t endPos = text.find(L")%", tokenPos + tokenPrefix.length());
            if (endPos != std::wstring::npos) {
                std::wstring symbolStr = text.substr(tokenPos + tokenPrefix.length(), endPos - tokenPos - tokenPrefix.length());
                std::wstring replacement = L"";
                
                for (wchar_t c : symbolStr) {
                    switch (c) {
                        case L'<': replacement += L"&lt;"; break;
                        case L'>': replacement += L"&gt;"; break;
                        case L'&': replacement += L"&amp;"; break;
                        case L'"': replacement += L"&quot;"; break;
                        case L'\'': replacement += L"&apos;"; break;
                        default: 
                            wchar_t buf[32];
                            if (useHex) {
                                swprintf_s(buf, ARRAYSIZE(buf), L"&#x%X;", (int)c);
                            } else {
                                swprintf_s(buf, ARRAYSIZE(buf), L"&#%d;", (int)c);
                            }
                            replacement += buf;
                            break;
                    }
                }
                // Replace the full token string with the resolved entities
                text.replace(tokenPos, endPos - tokenPos + 2, replacement);
                tokenPos += replacement.length();
            } else {
                tokenPos += tokenPrefix.length();
            }
        }
    };

    processHtmlToken(L"%html_dec(", false);
    processHtmlToken(L"%html_hex(", true);

    // Process explicit nested expansions %expand(trigger)%
    pos = 0;
    while ((pos = text.find(L"%expand(", pos)) != std::wstring::npos) {
        size_t endPos = text.find(L")%", pos + 8);
        if (endPos != std::wstring::npos) {
            std::wstring nestedTrigger = text.substr(pos + 8, endPos - pos - 8);
            std::wstring replacement = L"";
            
            // For standard matching, we need a lowercase version
            std::wstring lowerNestedTrigger = nestedTrigger;
            std::transform(lowerNestedTrigger.begin(), lowerNestedTrigger.end(), lowerNestedTrigger.begin(), std::towlower);
            
            bool matched = false;
            
            // Find the nested trigger in our mappings
            for (const auto& rule : g_expansions) {
                if (rule.isRegex && rule.isValidRegex) {
                    std::wsmatch match;
                    // For direct macro calls, we use regex_match to ensure the provided string fully satisfies the pattern
                    if (std::regex_match(nestedTrigger, match, rule.triggerRegex)) {
                        try {
                            replacement = match.format(rule.replacementText);
                            matched = true;
                            break;
                        } catch (...) {} // Ignore regex format errors in nested execution
                    }
                } else {
                    if (lowerNestedTrigger == rule.triggerText) {
                        replacement = rule.replacementText;
                        matched = true;
                        break;
                    }
                }
            }
            
            if (matched) {
                // Recursively process variables in the found replacement
                int dummyOffset = 0;
                replacement = ProcessVariables(replacement, dummyOffset, recursionDepth + 1);
            }
            
            // Replace the macro tag with the evaluated content (or empty string if no match was found)
            text.replace(pos, endPos - pos + 2, replacement);
            pos += replacement.length();
        } else {
            pos += 8;
        }
    }

    // Process %CURSOR% placement (Must be done LAST and only on the root evaluation level)
    if (recursionDepth == 0) {
        pos = text.find(L"%CURSOR%");
        if (pos != std::wstring::npos) {
            // Calculate how many characters are left after the token
            outCursorOffset = static_cast<int>(text.length() - pos - 8);
            
            // Remove the token from the final output
            text.replace(pos, 8, L"");
            
            // Failsafe in case user typed multiple %CURSOR% tokens (strip the rest)
            ReplaceAll(text, L"%CURSOR%", L"");
        }
    }

    return text;
}

// Dynamically iterates through the Windhawk settings array
void LoadSettings() {
    AcquireSRWLockExclusive(&g_lock);
    g_expansions.clear();
    g_maxHotstringLength = 0;
    
    PCWSTR symStr = Wh_GetStringSetting(L"activation_symbol");
    if (symStr) {
        g_activationSymbol = symStr;
        Wh_FreeStringSetting(symStr);
    }

    PCWSTR termStr = Wh_GetStringSetting(L"terminator_characters");
    if (termStr) {
        g_terminatorChars = termStr;
        ReplaceAll(g_terminatorChars, L"\\n", L"\n");
        ReplaceAll(g_terminatorChars, L"\\t", L"\t");
        ReplaceAll(g_terminatorChars, L"\\s", L" ");
        Wh_FreeStringSetting(termStr);
    } else {
        g_terminatorChars = L" ,.\n\t?!;:";
    }

    DebugLog(L"Loading settings. Symbol='%s', Terminators='%s'", g_activationSymbol.c_str(), g_terminatorChars.c_str());

    bool hasAnyRegex = false;

    for (int i = 0; ; i++) {
        WCHAR triggerKey[128];
        WCHAR replaceKey[128];
        WCHAR termKey[128];
        WCHAR instantKey[128];
        WCHAR regexKey[128];
        
        // Explicitly format the setting keys to bypass Windhawk macro quirks
        swprintf_s(triggerKey, ARRAYSIZE(triggerKey), L"expansions[%d].trigger", i);
        swprintf_s(replaceKey, ARRAYSIZE(replaceKey), L"expansions[%d].replacement", i);
        swprintf_s(termKey, ARRAYSIZE(termKey), L"expansions[%d].terminators", i);
        swprintf_s(instantKey, ARRAYSIZE(instantKey), L"expansions[%d].instant", i);
        swprintf_s(regexKey, ARRAYSIZE(regexKey), L"expansions[%d].is_regex", i);

        PCWSTR trigger = Wh_GetStringSetting(triggerKey);
        PCWSTR replace = Wh_GetStringSetting(replaceKey);
        PCWSTR customTermStr = Wh_GetStringSetting(termKey);
        bool isInstant = Wh_GetIntSetting(instantKey) != 0;
        bool isRegex = Wh_GetIntSetting(regexKey) != 0;

        // CHECK FIX: Ensure the pointer is valid AND the string is not empty
        bool hasRule = (trigger && *trigger != L'\0');

        if (!hasRule) {
            if (trigger) Wh_FreeStringSetting(trigger);
            if (replace) Wh_FreeStringSetting(replace);
            if (customTermStr) Wh_FreeStringSetting(customTermStr);
            break; // Reached the end of the configured array
        }

        std::wstring t(trigger);
        std::wstring r(replace ? replace : L"");
        std::wstring ruleTerms = g_terminatorChars; // Default to global terminators
        
        if (customTermStr) {
            std::wstring ct(customTermStr);
            if (!ct.empty()) {
                ReplaceAll(ct, L"\\n", L"\n");
                ReplaceAll(ct, L"\\t", L"\t");
                ReplaceAll(ct, L"\\s", L" ");
                ruleTerms = ct;
            }
            Wh_FreeStringSetting(customTermStr);
        }

        Wh_FreeStringSetting(trigger);
        if (replace) Wh_FreeStringSetting(replace);

        if (t.empty()) continue;
        
        // Translate literal "\n" strings from the UI into real newlines
        ReplaceAll(r, L"\\n", L"\n");
        
        bool isValidRegex = false;
        std::wregex compiledRegex;
        std::wstring finalTriggerText = t;
        
        if (isRegex) {
            std::wstring regexStr = t;
            // Prepend the activation symbol to the regex if not instant
            if (!isInstant) {
                regexStr = EscapeRegex(g_activationSymbol) + regexStr;
            }
            
            try {
                // Compile case-insensitively using the default ECMAScript grammar
                compiledRegex = std::wregex(regexStr, std::regex_constants::icase | std::regex_constants::ECMAScript);
                isValidRegex = true;
                hasAnyRegex = true;
            } catch (const std::regex_error& e) {
                DebugLog(L"Regex compilation failed for trigger '%s': %hs", t.c_str(), e.what());
                continue; // Skip invalid regex rules to prevent crashes
            }
        } else {
            if (!isInstant) {
                finalTriggerText = g_activationSymbol + t;
            }
            // Convert standard trigger to lowercase for case-insensitive matching
            std::transform(finalTriggerText.begin(), finalTriggerText.end(), finalTriggerText.begin(), std::towlower);
            
            if (finalTriggerText.length() > g_maxHotstringLength) {
                g_maxHotstringLength = finalTriggerText.length();
            }
        }
        
        g_expansions.push_back({finalTriggerText, compiledRegex, r, ruleTerms, isInstant, isRegex, isValidRegex});
        DebugLog(L"Loaded mapping -> '%s' (Instant: %d, Regex: %d)", t.c_str(), isInstant, isRegex);
    }
    
    // If we have regexes, we must allow the memory buffer to grow larger to accommodate complex matching. 
    // 1024 characters is a safe limit that prevents memory leaks while catching long strings.
    if (hasAnyRegex) {
        g_maxHotstringLength = std::max<size_t>(g_maxHotstringLength, 1024);
    }

    DebugLog(L"Settings load complete. Total mappings: %zu, Max Buffer Size: %zu", g_expansions.size(), g_maxHotstringLength);
    ReleaseSRWLockExclusive(&g_lock);
}

void SendBackspace(int count) {
    if (count <= 0) return;
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

void SendLeftArrow(int count) {
    if (count <= 0) return;
    std::vector<INPUT> inputs;
    for (int i = 0; i < count; ++i) {
        INPUT ip = {0};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = VK_LEFT;
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
        
        bool isSpecial = true;
        DWORD vkCode = 0;
        DWORD flags = 0;

        switch (c) {
            case L'\n':     vkCode = VK_RETURN; break;
            case L'\t':     vkCode = VK_TAB; break;
            case L'\x1B':   vkCode = VK_ESCAPE; break;
            case L'\xE000': vkCode = VK_BACK; break;
            case L'\xE001': vkCode = VK_HOME; flags = KEYEVENTF_EXTENDEDKEY; break;
            case L'\xE002': vkCode = VK_END; flags = KEYEVENTF_EXTENDEDKEY; break;
            case L'\xE003': vkCode = VK_NEXT; flags = KEYEVENTF_EXTENDEDKEY; break; // Page Down
            case L'\xE004': vkCode = VK_PRIOR; flags = KEYEVENTF_EXTENDEDKEY; break; // Page Up
            case L'\xE005': vkCode = VK_DELETE; flags = KEYEVENTF_EXTENDEDKEY; break;
            case L'\xE006': vkCode = VK_MEDIA_PLAY_PAUSE; flags = KEYEVENTF_EXTENDEDKEY; break;
            case L'\xE007': vkCode = VK_MEDIA_PREV_TRACK; flags = KEYEVENTF_EXTENDEDKEY; break;
            case L'\xE008': vkCode = VK_MEDIA_NEXT_TRACK; flags = KEYEVENTF_EXTENDEDKEY; break;
            case L'\xE009': vkCode = VK_VOLUME_UP; flags = KEYEVENTF_EXTENDEDKEY; break;
            case L'\xE00A': vkCode = VK_VOLUME_DOWN; flags = KEYEVENTF_EXTENDEDKEY; break;
            default:        isSpecial = false; break;
        }

        if (isSpecial) {
            INPUT ip = {0};
            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = (WORD)vkCode;
            ip.ki.dwFlags = flags;
            inputs.push_back(ip);
            
            ip.ki.dwFlags = flags | KEYEVENTF_KEYUP;
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
        case VK_RETURN:    return L'\n';
        case VK_TAB:       return L'\t';
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
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        
        // Ignore keystrokes injected by this mod or other software to prevent loops
        if (pKeyBoard->flags & LLKHF_INJECTED) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

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
            
            std::wstring currentBuffer = g_buffer;
            
            // Create a lowercase copy of the buffer for case-insensitive matching (used by standard triggers)
            std::wstring currentBufferLower = currentBuffer;
            std::transform(currentBufferLower.begin(), currentBufferLower.end(), currentBufferLower.begin(), std::towlower);
            
            bool matched = false;
            int backspaceCount = 0;
            std::wstring replacementToProcess;
            bool keepTerminator = false;

            for (const auto& rule : g_expansions) {
                bool evaluate = false;
                std::wstring matchTarget = currentBuffer;
                std::wstring matchTargetLower = currentBufferLower;

                if (rule.isInstant) {
                    evaluate = true;
                    keepTerminator = false;
                } else {
                    // Non-instant. Check if the currently typed character 'c' is a terminator for THIS specific rule.
                    if (rule.terminators.find(c) != std::wstring::npos) {
                        evaluate = true;
                        keepTerminator = true;
                        matchTarget.pop_back(); // Remove the terminator for pure trigger matching
                        matchTargetLower.pop_back();
                    }
                }

                if (!evaluate) continue;

                if (rule.isRegex && rule.isValidRegex) {
                    DebugLog(L"Evaluating regex trigger '%s' against buffer '%s'", rule.triggerText.c_str(), matchTarget.c_str());
                    
                    std::wsmatch match;
                    std::wstring::const_iterator searchStart = matchTarget.cbegin();
                    bool found = false;
                    
                    // We search the entire physical buffer. If the regex successfully matches the 
                    // exact END of the buffer (suffix length == 0), then the trigger conditions are met.
                    while (std::regex_search(searchStart, matchTarget.cend(), match, rule.triggerRegex)) {
                        if (match.suffix().length() == 0) {
                            found = true;
                            break;
                        }
                        if (match.length() == 0) {
                            searchStart++;
                            if (searchStart == matchTarget.cend()) break;
                        } else {
                            searchStart = match.suffix().first;
                        }
                    }
                    
                    if (found && match.length() > 0) {
                        DebugLog(L"Regex match found! Matched string: '%s'", match.str().c_str());
                        backspaceCount = static_cast<int>(match.length());
                        
                        try {
                            // Execute regex format to process user capture groups ($1, $2)
                            replacementToProcess = match.format(rule.replacementText);
                            DebugLog(L"Formatted replacement string: '%s'", replacementToProcess.c_str());
                            matched = true;
                            break;
                        } catch (const std::exception& e) {
                            // This catch block prevents missing capture groups (like $2) from crashing the thread
                            DebugLog(L"Regex format error: %hs. Check your replacement groups.", e.what());
                        }
                    }
                } else {
                    if (matchTargetLower.length() >= rule.triggerText.length() && 
                        matchTargetLower.compare(matchTargetLower.length() - rule.triggerText.length(), rule.triggerText.length(), rule.triggerText) == 0) {
                        
                        DebugLog(L"Standard match found for '%s', expanding!", rule.triggerText.c_str());
                        backspaceCount = static_cast<int>(rule.triggerText.length());
                        replacementToProcess = rule.replacementText;
                        matched = true;
                        break;
                    }
                }
            }

            if (matched) {
                g_buffer.clear();
                
                int cursorOffset = 0;
                // Parse standard macros (%DATE%, %TAB%, %CURSOR%, etc.)
                std::wstring finalOutput = ProcessVariables(replacementToProcess, cursorOffset);
                
                if (keepTerminator) {
                    // Erase the typed characters (the terminator is blocked, so we only erase the trigger length)
                    SendBackspace(backspaceCount);
                    SendString(finalOutput);
                    
                    // Append the intercepted terminator back onto the screen
                    std::wstring termStr(1, c);
                    SendString(termStr);
                    
                    // Adjust cursor to account for the appended terminator
                    if (cursorOffset > 0) {
                        cursorOffset += 1;
                    }
                    SendLeftArrow(cursorOffset);
                } else {
                    // Erase the typed characters (-1 because we block the current keydown from registering)
                    SendBackspace(backspaceCount - 1);
                    SendString(finalOutput);
                    SendLeftArrow(cursorOffset);
                }
                
                ReleaseSRWLockShared(&g_lock);
                return 1; // Block the final trigger keystroke from reaching the active window
            }

            ReleaseSRWLockShared(&g_lock);
            
        } else if (vkCode != VK_SHIFT && vkCode != VK_LSHIFT && vkCode != VK_RSHIFT && 
                   vkCode != VK_CONTROL && vkCode != VK_LCONTROL && vkCode != VK_RCONTROL && 
                   vkCode != VK_MENU && vkCode != VK_LMENU && vkCode != VK_RMENU && 
                   vkCode != VK_CAPITAL && vkCode != VK_LWIN && vkCode != VK_RWIN) {
            // Unmapped control keys (like Arrow keys, Escape) clear the buffer so triggers don't wrap across lines
            g_buffer.clear(); 
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

DWORD WINAPI HookThread(LPVOID lpParam) {
    HMODULE hModule = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      (PCWSTR)&KeyboardProc, &hModule);
                      
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hModule, 0);
    if (!g_hHook) {
        DebugLog(L"FAILED to set WH_KEYBOARD_LL hook! Error: %lu", GetLastError());
    } else {
        DebugLog(L"Successfully registered WH_KEYBOARD_LL hook.");
    }
    
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {
            break;
        }
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
    DebugLog(L"Settings changed, reloading...");
    LoadSettings();
}

BOOL Wh_ModInit() {
    DebugLog(L"Initializing Text Expansion mod in explorer.exe...");
    LoadSettings();
    g_hThread = CreateThread(NULL, 0, HookThread, NULL, 0, &g_threadId);
    return TRUE;
}

void Wh_ModUninit() {
    DebugLog(L"Uninitializing Text Expansion mod...");
    if (g_threadId) {
        PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_hThread, 1000);
        CloseHandle(g_hThread);
        g_hThread = NULL;
        g_threadId = 0;
    }
}