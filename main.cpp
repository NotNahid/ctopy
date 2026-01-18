#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

// Global Variables
std::vector<std::wstring> clipHistory;
HWND hList;
HWND hMainWnd;
const int HOTKEY_ID = 1;
const wchar_t* DB_FILE = L"clipboard_history.txt";

// Helper: Save to file
void SaveHistory() {
    std::wofstream file(DB_FILE);
    for (const auto& clip : clipHistory) {
        // Escape newlines for simple storage
        std::wstring temp = clip;
        std::replace(temp.begin(), temp.end(), L'\n', L'|');
        file << temp << L"\n";
    }
}

// Helper: Load from file
void LoadHistory() {
    std::wifstream file(DB_FILE);
    std::wstring line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::replace(line.begin(), line.end(), L'|', L'\n');
        clipHistory.push_back(line);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

// Helper: Set Clipboard Text
void SetClipboard(const std::wstring& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(wchar_t));
        if (hGlob) {
            memcpy(GlobalLock(hGlob), text.c_str(), (text.length() + 1) * sizeof(wchar_t));
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
        CloseClipboard();
    }
}

// Helper: Simulate Ctrl+V
void SendCtrlV() {
    INPUT ip[4] = {};
    ip[0].type = INPUT_KEYBOARD; ip[0].ki.wVk = VK_CONTROL;
    ip[1].type = INPUT_KEYBOARD; ip[1].ki.wVk = 'V';
    ip[2].type = INPUT_KEYBOARD; ip[2].ki.wVk = 'V'; ip[2].ki.dwFlags = KEYEVENTF_KEYUP;
    ip[3].type = INPUT_KEYBOARD; ip[3].ki.wVk = VK_CONTROL; ip[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, ip, sizeof(INPUT));
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // Create the ListBox
        hList = CreateWindowW(L"LISTBOX", NULL, 
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
            0, 0, 400, 500, hwnd, (HMENU)101, NULL, NULL);
        
        // Setup Features
        RegisterHotKey(hwnd, HOTKEY_ID, MOD_CONTROL | MOD_SHIFT, 'V');
        AddClipboardFormatListener(hwnd);
        LoadHistory();
        break;

    case WM_SIZE:
        MoveWindow(hList, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        break;

    case WM_CLIPBOARDUPDATE:
        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            if (OpenClipboard(hwnd)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
                    if (pszText) {
                        std::wstring text(pszText);
                        GlobalUnlock(hData);
                        
                        // Avoid duplicates at the top
                        if (clipHistory.empty() || clipHistory.back() != text) {
                            clipHistory.push_back(text);
                            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)text.c_str());
                            SendMessageW(hList, WM_VSCROLL, SB_BOTTOM, 0); // Scroll to bottom
                            SaveHistory();
                        }
                    }
                }
                CloseClipboard();
            }
        }
        break;

    case WM_HOTKEY:
        if (wParam == HOTKEY_ID) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 101 && HIWORD(wParam) == LBN_DBLCLK) {
            int idx = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (idx != LB_ERR) {
                // User picked an item
                std::wstring selected = clipHistory[idx];
                SetClipboard(selected);
                ShowWindow(hwnd, SW_HIDE);
                Sleep(100);
                SendCtrlV();
            }
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE); // Don't close, just hide
        return 0;

    case WM_DESTROY:
        RemoveClipboardFormatListener(hwnd);
        UnregisterHotKey(hwnd, HOTKEY_ID);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"SmartClipboardClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    // Icon (Try to load app.ico if it exists)
    wc.hIcon = (HICON)LoadImage(NULL, L"app.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

    RegisterClassW(&wc);

    hMainWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, CLASS_NAME, L"Smart Clipboard Pro",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 500,
        NULL, NULL, hInstance, NULL);

    // Main Loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
