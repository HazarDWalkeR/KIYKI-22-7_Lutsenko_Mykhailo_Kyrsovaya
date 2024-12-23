#include <windows.h>
#include <psapi.h>
#include <tchar.h>
#include <stdio.h>
#include <ctime>
#include <shellapi.h>
#include <ShellScalingApi.h>
#pragma comment(lib, "Shcore.lib")

#define ID_TIMER 1
#define ID_STATIC_VIRTUAL_TOTAL 101
#define ID_STATIC_VIRTUAL_USED 102
#define ID_STATIC_PHYSICAL_TOTAL 103
#define ID_STATIC_PHYSICAL_USED 104
#define ID_STATIC_CPU_USAGE 105

UINT updateInterval = 100; // Початкова частота оновлення
bool isLoggingEnabled = false; // Флаг для контролю логування
FILE* logFile = NULL; // Файл для запису логів

typedef struct {
    ULONGLONG idleTime;
    ULONGLONG kernelTime;
    ULONGLONG userTime;
} SYSTEMTIMES;

void GetMemoryStatus(TCHAR* virtualTotal, TCHAR* virtualUsed, TCHAR* physicalTotal, TCHAR* physicalUsed, size_t bufferSize) {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    _stprintf_s(virtualTotal, bufferSize, _T("Загальна віртуальна пам'ять: %llu МБ"), memInfo.ullTotalPageFile / (1024 * 1024));
    _stprintf_s(virtualUsed, bufferSize, _T("Використано віртуальної пам'яті: %llu МБ"), (memInfo.ullTotalPageFile - memInfo.ullAvailPageFile) / (1024 * 1024));
    _stprintf_s(physicalTotal, bufferSize, _T("Загальна фізична пам'ять: %llu МБ"), memInfo.ullTotalPhys / (1024 * 1024));
    _stprintf_s(physicalUsed, bufferSize, _T("Використано фізичної пам'яті: %llu МБ"), (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024));
}

void GetSystemTimes(SYSTEMTIMES* sysTimes) {
    FILETIME idleTime, kernelTime, userTime;
    GetSystemTimes(&idleTime, &kernelTime, &userTime);

    sysTimes->idleTime = ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;
    sysTimes->kernelTime = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
    sysTimes->userTime = ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;
}

double CalculateCPUUsage(SYSTEMTIMES* prevTimes, SYSTEMTIMES* curTimes) {
    ULONGLONG idleDelta = curTimes->idleTime - prevTimes->idleTime;
    ULONGLONG kernelDelta = curTimes->kernelTime - prevTimes->kernelTime;
    ULONGLONG userDelta = curTimes->userTime - prevTimes->userTime;
    ULONGLONG totalSys = kernelDelta + userDelta;

    if (totalSys == 0) {
        return 0.0;
    }

    return (1.0 - ((double)idleDelta / totalSys)) * 100.0;
}

void GetCPUUsage(TCHAR* cpuUsage, SYSTEMTIMES* prevTimes, SYSTEMTIMES* curTimes, size_t bufferSize) {
    GetSystemTimes(curTimes);
    double usage = CalculateCPUUsage(prevTimes, curTimes);
    _stprintf_s(cpuUsage, bufferSize, _T("Використання ЦПУ: %.2f %%"), usage);
    *prevTimes = *curTimes;
}

void StartLogging() {
    if (logFile == NULL) {
        _tfopen_s(&logFile, _T("MemoryLog.txt"), _T("w, ccs=UTF-8"));
        if (logFile != NULL) {
            fputwc(0xFEFF, logFile); // Додаем BOM для UTF-8
        }
    }
}

void StopLogging() {
    if (logFile != NULL) {
        fclose(logFile);
        logFile = NULL;
    }
}

void LogMemoryAndCPUStatus(TCHAR* virtualTotal, TCHAR* virtualUsed, TCHAR* physicalTotal, TCHAR* physicalUsed, TCHAR* cpuUsage) {
    if (logFile != NULL) {
        static int logIndex = 1;
        time_t currentTime = time(NULL);
        struct tm localTime;
        localtime_s(&localTime, &currentTime);

        TCHAR timeString[64];
        _tcsftime(timeString, sizeof(timeString) / sizeof(TCHAR), _T("%Y-%m-%d %H:%M:%S"), &localTime);

        _ftprintf(logFile, _T("%d. [%s] %s, %s, %s, %s, %s\n"), logIndex, timeString, virtualTotal, virtualUsed, physicalTotal, physicalUsed, cpuUsage);
        logIndex++;
        fflush(logFile);
    }
}

void OpenLogFile() {
    ShellExecute(NULL, _T("open"), _T("MemoryLog.txt"), NULL, NULL, SW_SHOW);
}

void ChangeUpdateInterval(HWND hWnd, UINT interval) {
    KillTimer(hWnd, ID_TIMER);
    SetTimer(hWnd, ID_TIMER, interval, NULL);
}

void SetDpiAwareness() {
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static TCHAR virtualTotal[256], virtualUsed[256], physicalTotal[256], physicalUsed[256], cpuUsage[256];
    static SYSTEMTIMES prevTimes = { 0 }, curTimes = { 0 };

    switch (message) {
    case WM_CREATE:
        SetTimer(hWnd, ID_TIMER, updateInterval, NULL);
        CreateWindow(_T("STATIC"), _T("Загальна віртуальна пам'ять: "), WS_VISIBLE | WS_CHILD, 10, 10, 300, 20, hWnd, (HMENU)ID_STATIC_VIRTUAL_TOTAL, NULL, NULL);
        CreateWindow(_T("STATIC"), _T("Використано віртуальної пам'яті: "), WS_VISIBLE | WS_CHILD, 10, 40, 300, 20, hWnd, (HMENU)ID_STATIC_VIRTUAL_USED, NULL, NULL);
        CreateWindow(_T("STATIC"), _T("Загальна фізична пам'ять: "), WS_VISIBLE | WS_CHILD, 10, 70, 300, 20, hWnd, (HMENU)ID_STATIC_PHYSICAL_TOTAL, NULL, NULL);
        CreateWindow(_T("STATIC"), _T("Використано фізичної пам'яті: "), WS_VISIBLE | WS_CHILD, 10, 100, 300, 20, hWnd, (HMENU)ID_STATIC_PHYSICAL_USED, NULL, NULL);
        CreateWindow(_T("STATIC"), _T("Використання ЦПУ: "), WS_VISIBLE | WS_CHILD, 10, 130, 300, 20, hWnd, (HMENU)ID_STATIC_CPU_USAGE, NULL, NULL);
        CreateWindow(_T("BUTTON"), _T("Частота 100 мс"), WS_VISIBLE | WS_CHILD | BS_LEFT, 320, 10, 150, 25, hWnd, (HMENU)200, NULL, NULL);
        CreateWindow(_T("BUTTON"), _T("Частота 1000 мс"), WS_VISIBLE | WS_CHILD | BS_LEFT, 320, 40, 150, 25, hWnd, (HMENU)201, NULL, NULL);
        CreateWindow(_T("BUTTON"), _T("Записати показники"), WS_VISIBLE | WS_CHILD | BS_LEFT, 320, 70, 150, 25, hWnd, (HMENU)202, NULL, NULL);
        CreateWindow(_T("BUTTON"), _T("Стоп записи"), WS_VISIBLE | WS_CHILD | BS_LEFT, 320, 100, 150, 25, hWnd, (HMENU)203, NULL, NULL);
        CreateWindow(_T("BUTTON"), _T("Відкрити лог"), WS_VISIBLE | WS_CHILD | BS_LEFT, 320, 130, 150, 25, hWnd, (HMENU)204, NULL, NULL);
        GetSystemTimes(&prevTimes);
        return 0;

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        return 1;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER) {
            GetMemoryStatus(virtualTotal, virtualUsed, physicalTotal, physicalUsed, sizeof(virtualTotal) / sizeof(TCHAR));
            GetCPUUsage(cpuUsage, &prevTimes, &curTimes, sizeof(cpuUsage) / sizeof(TCHAR));
            SetWindowText(GetDlgItem(hWnd, ID_STATIC_VIRTUAL_TOTAL), virtualTotal);
            SetWindowText(GetDlgItem(hWnd, ID_STATIC_VIRTUAL_USED), virtualUsed);
            SetWindowText(GetDlgItem(hWnd, ID_STATIC_PHYSICAL_TOTAL), physicalTotal);
            SetWindowText(GetDlgItem(hWnd, ID_STATIC_PHYSICAL_USED), physicalUsed);
            SetWindowText(GetDlgItem(hWnd, ID_STATIC_CPU_USAGE), cpuUsage);

            if (isLoggingEnabled) {
                LogMemoryAndCPUStatus(virtualTotal, virtualUsed, physicalTotal, physicalUsed, cpuUsage);
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 200:
            updateInterval = 100;
            ChangeUpdateInterval(hWnd, updateInterval);
            break;
        case 201:
            updateInterval = 1000;
            ChangeUpdateInterval(hWnd, updateInterval);
            break;
        case 202:
            isLoggingEnabled = true;
            StartLogging();
            break;
        case 203:
            isLoggingEnabled = false;
            StopLogging();
            break;
        case 204:
            OpenLogFile();
            break;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER);
        StopLogging();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    SetDpiAwareness();

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("MemoryCPUApp");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Встановлюємо білий фон
    RegisterClass(&wc);

    HWND hWnd = CreateWindow(_T("MemoryCPUApp"), _T("Моніторинг пам'яті та CPU"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 300, NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
