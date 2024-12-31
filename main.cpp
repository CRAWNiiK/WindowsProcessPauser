#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

constexpr int MAX_PROCESS_NAME_LEN = 256;
constexpr int MAX_TIME_INPUT_LEN = 10;
constexpr int PROGRESS_BAR_STEPS = 100;

DWORD processPid = 0;
bool isPaused = false;
static std::string pauseButtonText = "Pause";

// Suspend or resume a process using NtSuspendProcess or NtResumeProcess
void SuspendOrResumeProcess(DWORD pid, bool suspend) {
    HANDLE processHandle = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!processHandle) {
        throw std::runtime_error("Failed to open process with PID " + std::to_string(pid));
    }

    const char* functionName = suspend ? "NtSuspendProcess" : "NtResumeProcess";
    typedef LONG(NTAPI* NtProcess)(HANDLE ProcessHandle);
    NtProcess processFunc = (NtProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), functionName);

    if (!processFunc || processFunc(processHandle) != 0) {
        CloseHandle(processHandle);
        throw std::runtime_error("Failed to " + std::string(suspend ? "suspend" : "resume") + " process with PID " + std::to_string(pid));
    }

    CloseHandle(processHandle);
}

// Find the process ID by its name
DWORD FindProcessId(const std::string& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to take process snapshot");
    }

    PROCESSENTRY32 processEntry = { sizeof(PROCESSENTRY32) };
    if (Process32First(snapshot, &processEntry)) {
        do {
            if (processName == processEntry.szExeFile) {
                CloseHandle(snapshot);
                return processEntry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    throw std::runtime_error("Process '" + processName + "' not found.");
}

// Custom drawing for buttons (custom draw)
void DrawButton(HDC hdc, RECT* rect, BOOL isPressed, BOOL isFocused) {
    // Adjust the radius for rounded corners
    int cornerRadius = 10;

    // Create a brush with a color depending on whether the button is pressed
    HBRUSH hBrush = CreateSolidBrush(isPressed ? RGB(120, 81, 169) : RGB(57, 57, 60)); // Change color here
    SelectObject(hdc, hBrush);
    RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, cornerRadius, cornerRadius);
    DeleteObject(hBrush);

    // Set the text color
    SetTextColor(hdc, RGB(255, 255, 255)); 
    SetBkMode(hdc, TRANSPARENT);

    // Draw the button text
    DrawText(hdc, pauseButtonText.c_str(), -1, rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND processNameEdit, pauseButton, progressBar, timeEdit, closeButton, titleText;
    static bool isPaused = false;

    // Declare isDragging and offset
    static bool isDragging = false;
    static POINT offset;

    // Handle custom drawing of the buttons
    if (uMsg == WM_DRAWITEM) {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;

        if (dis->CtlID == 1) {  // Pause button
            DrawButton(dis->hDC, &dis->rcItem, (dis->itemState & ODS_SELECTED), (dis->itemState & ODS_FOCUS));
            DrawText(dis->hDC, pauseButtonText.c_str(), -1, &dis->rcItem, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            return TRUE;  // Custom drawing handled for Pause button
        }
        else if (dis->CtlID == 2) {  // Close button (X)
			// Define custom drawing for the "X" button here
			HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0)); // Red background for the "X" button
			SelectObject(dis->hDC, hBrush);
		
			// Set the radius for the rounded corners
			int cornerRadius = 10;
			RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, cornerRadius, cornerRadius);
		
			// Draw the "X" text in the center
			SetTextColor(dis->hDC, RGB(255, 255, 255)); // White text color
			SetBkMode(dis->hDC, TRANSPARENT);
			DrawText(dis->hDC, "X", -1, &dis->rcItem, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			
			// Cleanup
			DeleteObject(hBrush);
		
			return TRUE;  // Custom drawing handled for "X" button
		}
    }

    switch (uMsg) {
        case WM_CREATE: {
			// Get the screen width and height
			int screenWidth = GetSystemMetrics(SM_CXSCREEN);
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		
			// Set the window width and height
			int windowWidth = 300;
			int windowHeight = 180;
		
			// Calculate the position to center the window
			int xPos = (screenWidth - windowWidth) / 2;
			int yPos = (screenHeight - windowHeight) / 2;
		
			// Remove the title bar and disable resizing
			LONG style = GetWindowLong(hwnd, GWL_STYLE);
			style &= ~(WS_CAPTION | WS_SIZEBOX); // Remove title bar and resize box
			SetWindowLong(hwnd, GWL_STYLE, style);
		
			HBRUSH hBrush = CreateSolidBrush(RGB(45, 45, 48));
			SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);
		
			// Set the window position to the center of the screen
			SetWindowPos(hwnd, HWND_TOP, xPos, yPos, windowWidth, windowHeight, SWP_NOZORDER);
		
			// Create rounded corners for the window
			HRGN hRgn = CreateRoundRectRgn(0, 0, windowWidth, windowHeight, 20, 20); // 20 is the corner radius
			SetWindowRgn(hwnd, hRgn, TRUE); // Apply the rounded region to the window
		
			// Create the UI controls without a title bar
			processNameEdit = CreateWindow("EDIT", "GTA5.exe", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 120, 40, 150, 20, hwnd, NULL, NULL, NULL);
			CreateWindow("STATIC", "Process Name:", WS_VISIBLE | WS_CHILD, 10, 40, 100, 20, hwnd, NULL, NULL, NULL);
		
			CreateWindow("STATIC", "Time (s):", WS_VISIBLE | WS_CHILD, 10, 70, 100, 20, hwnd, NULL, NULL, NULL);
			timeEdit = CreateWindow("EDIT", "10", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 120, 70, 150, 20, hwnd, NULL, NULL, NULL);
		
			pauseButton = CreateWindow("BUTTON", "Pause", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 10, 100, 100, 30, hwnd, (HMENU)1, NULL, NULL);
		
			// Progress bar creation using CommCtrl
			progressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 120, 100, 150, 30, hwnd, (HMENU)3, NULL, NULL);
			SendMessage(progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, PROGRESS_BAR_STEPS));
			SendMessage(progressBar, PBM_SETSTEP, (WPARAM)1, 0);
			SendMessage(progressBar, PBM_SETBKCOLOR, 0, (LPARAM)RGB(45, 45, 48));
			SendMessage(progressBar, PBM_SETBARCOLOR, 0, (LPARAM)RGB(120, 81, 169));
		
			CreateWindow("STATIC", "Made by CRAWNiiK", WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 140, 300, 20, hwnd, NULL, NULL, NULL);
		
			// Create the "Process Pauser" text and center it
			titleText = CreateWindow("STATIC", "Process Pauser", WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 10, 250, 20, hwnd, NULL, NULL, NULL);
		
			// Create custom "X" button for closing the window with the custom drawing
			closeButton = CreateWindow("BUTTON", "X", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 270, 10, 20, 20, hwnd, (HMENU)2, NULL, NULL);
			break;
		}

        case WM_CTLCOLORSTATIC: {
			HDC hdcStatic = (HDC)wParam;
		
			// Set the text color for static controls (labels)
			SetTextColor(hdcStatic, RGB(255, 255, 255));
		
			// Set the background color to match the window's background
			SetBkColor(hdcStatic, RGB(45, 45, 48));
		
			// Use a null brush to prevent default background drawing
			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
        case WM_ERASEBKGND: {
			HDC hdc = (HDC)wParam;
			RECT rect;
			GetClientRect(hwnd, &rect); // Ensure we target the entire window
			FillRect(hdc, &rect, CreateSolidBrush(RGB(45, 45, 48))); // Set the background color
			return 1; // Indicate the background was erased
		}
		
		case WM_CTLCOLOREDIT: {
			HDC hdcEdit = (HDC)wParam;
			SetBkColor(hdcEdit, RGB(45, 45, 48)); // Set background color for edit controls
			SetTextColor(hdcEdit, RGB(255, 255, 255)); // Set text color
			return (LRESULT)GetStockObject(NULL_BRUSH); // Prevent default background painting
		}
        case WM_CTLCOLORBTN: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (INT_PTR)GetStockObject(NULL_BRUSH);
        }

        case WM_COMMAND:
			if (LOWORD(wParam) == 1) { // Pause Button
				if (isPaused) return 0;
		
				char processName[MAX_PROCESS_NAME_LEN];
				GetWindowText(processNameEdit, processName, MAX_PROCESS_NAME_LEN);
		
				char timeBuffer[MAX_TIME_INPUT_LEN];
				GetWindowText(timeEdit, timeBuffer, MAX_TIME_INPUT_LEN);
				int pauseTime = std::min(std::max(atoi(timeBuffer), 1), 999);
		
				try {
					processPid = FindProcessId(processName);
					SuspendOrResumeProcess(processPid, true);
					isPaused = true;
		
					pauseButtonText = "Suspending";
					InvalidateRect(pauseButton, NULL, TRUE);
					UpdateWindow(pauseButton);
					EnableWindow(pauseButton, FALSE);
		
					std::thread([hwnd, pauseTime]() {
						for (int i = 0; i <= PROGRESS_BAR_STEPS; ++i) {
							Sleep(pauseTime * 10);
							SendMessage(progressBar, PBM_SETPOS, i, 0);
		
							// Check if the process is still running every 100ms
							DWORD exitCode;
							HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processPid);
							if (processHandle && GetExitCodeProcess(processHandle, &exitCode)) {
								if (exitCode != STILL_ACTIVE) {
									// Process is no longer running, reset UI
									PostMessage(hwnd, WM_USER + 2, 0, 0); // Notify the UI
									CloseHandle(processHandle);
									return;
								}
							}
							CloseHandle(processHandle);
						}
						SuspendOrResumeProcess(processPid, false);
						PostMessage(hwnd, WM_USER + 1, 0, 0);
					}).detach();
				} catch (const std::exception& e) {
					MessageBox(hwnd, e.what(), "Error", MB_ICONERROR);
				}
			}
			else if (LOWORD(wParam) == 2) { // Close Button (X)
				if (isPaused) {  // Check if the process is paused
					try {
						// Resume the process if it's paused
						SuspendOrResumeProcess(processPid, false); // false for resume
						isPaused = false;  // Update the paused state
						pauseButtonText = "Closing";  // Update button text to reflect that the process is resumed
						InvalidateRect(pauseButton, NULL, TRUE); // Force update of the button's text
						UpdateWindow(pauseButton);
					}
					catch (const std::exception& ex) {
						MessageBoxA(hwnd, ex.what(), "Error Resuming Process", MB_ICONERROR);
					}
				}
				DestroyWindow(hwnd);  // Close the window
			}
			break;
		
		case WM_USER + 1:  // Process resumed
			pauseButtonText = "Pause";
			InvalidateRect(pauseButton, NULL, TRUE);
			UpdateWindow(pauseButton);
			EnableWindow(pauseButton, TRUE);
			SendMessage(progressBar, PBM_SETPOS, 0, 0);
			isPaused = false;
			break;
		
		case WM_USER + 2:  // Process no longer running
			pauseButtonText = "Pause";
			InvalidateRect(pauseButton, NULL, TRUE);
			UpdateWindow(pauseButton);
			EnableWindow(pauseButton, TRUE);
			SendMessage(progressBar, PBM_SETPOS, 0, 0);
			isPaused = false;
			MessageBox(hwnd, "The process has been terminated.", "Error", MB_ICONERROR);
			break;

        case WM_CHAR:
			if (lParam == (LPARAM)processNameEdit || lParam == (LPARAM)timeEdit) {
				HWND editControl = (HWND)lParam;
		
				if (wParam == VK_BACK) {
					int length = GetWindowTextLength(editControl);
					if (length > 0) {
						char buffer[MAX_PATH];
						GetWindowText(editControl, buffer, MAX_PATH);
						buffer[length - 1] = '\0';  // Remove the last character
						SetWindowText(editControl, buffer);
		
						// Force clear and redraw
						InvalidateRect(editControl, NULL, TRUE);
						UpdateWindow(editControl);
					}
				} else {
					// Handle appending new text
					char buffer[MAX_PATH];
					int length = GetWindowTextLength(editControl);
					GetWindowText(editControl, buffer, MAX_PATH);
		
					if (length < MAX_PATH - 1) {
						buffer[length] = (char)wParam;
						buffer[length + 1] = '\0';
						SetWindowText(editControl, buffer);
		
						// Force clear and redraw
						InvalidateRect(editControl, NULL, TRUE);
						UpdateWindow(editControl);
					}
				}
				return 0;
			}
			break;

        case WM_MOUSEMOVE:
            if (isDragging) {
                POINT pt;
                GetCursorPos(&pt);
                SetWindowPos(hwnd, NULL, pt.x - offset.x, pt.y - offset.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
            }
            break;

        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            isDragging = true;
            GetCursorPos(&offset);
            ScreenToClient(hwnd, &offset);
            break;

        case WM_LBUTTONUP:
            ReleaseCapture();
            isDragging = false;
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ProcessPauserClass";

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Window registration failed!", "Error", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, "Process Pauser", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBox(NULL, "Window creation failed!", "Error", MB_ICONERROR);
        return 0;
    }

    // Initialize COM controls (for progress bar, etc.)
    InitCommonControls();

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
