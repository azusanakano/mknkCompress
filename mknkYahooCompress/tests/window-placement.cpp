#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../src/window-placement.h"

namespace {
void Require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}

struct TestIni {
  wchar_t path[MAX_PATH]{};
  TestIni() {
    wchar_t directory[MAX_PATH]{};
    Require(GetTempPathW(MAX_PATH, directory) != 0, "temporary directory");
    Require(GetTempFileNameW(directory, L"mkp", 0, path) != 0, "temporary INI");
  }
  ~TestIni() { WritePrivateProfileStringW(nullptr, nullptr, nullptr, path); DeleteFileW(path); }
};

struct TestWindow {
  HWND hwnd{};
  TestWindow() { Reset(); }
  ~TestWindow() { if (hwnd) DestroyWindow(hwnd); }
  void Reset() {
    if (hwnd) DestroyWindow(hwnd);
    hwnd = CreateWindowExW(0, L"mknkPlacementTest", L"Window placement test",
                          WS_OVERLAPPEDWINDOW, 60, 80, 420, 300,
                          nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Require(hwnd != nullptr, "create test window");
  }
};

WINDOWPLACEMENT Placement(HWND hwnd) {
  WINDOWPLACEMENT result{}; result.length = sizeof(result);
  Require(GetWindowPlacement(hwnd, &result) != FALSE, "get placement");
  return result;
}

void CheckBounds(HWND hwnd, const RECT& expected) {
  auto actual = Placement(hwnd).rcNormalPosition;
  Require(EqualRect(&actual, &expected) != FALSE, "normal position/size changed");
}

void Restart(TestWindow& window, const TestIni& ini, bool wasMaximized = false) {
  Require(window_placement::Save(window.hwnd, ini.path, wasMaximized), "save placement");
  window.Reset();
  Require(window_placement::Restore(window.hwnd, ini.path), "restore placement");
  Require(IsWindowVisible(window.hwnd) != FALSE, "restored window is hidden");
  Require(IsIconic(window.hwnd) == FALSE, "restored window is minimized");
}
}

int main() {
  try {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"mknkPlacementTest";
    Require(RegisterClassW(&wc) != 0, "register test window");
    TestIni ini;
    TestWindow window;

    Require(!window_placement::Restore(window.hwnd, ini.path), "missing setting must use defaults");
    std::cout << "PASS: first launch without placement\n";

    ShowWindow(window.hwnd, SW_SHOWNORMAL);
    SetWindowPos(window.hwnd, nullptr, 95, 125, 540, 380, SWP_NOZORDER);
    const auto normal = Placement(window.hwnd).rcNormalPosition;
    Restart(window, ini);
    CheckBounds(window.hwnd, normal);
    Require(!IsZoomed(window.hwnd), "normal state");
    // Repeat to detect taskbar coordinate drift.
    Restart(window, ini); CheckBounds(window.hwnd, normal);
    std::cout << "PASS: normal position/size survives repeated restart\n";

    ShowWindow(window.hwnd, SW_SHOWMAXIMIZED);
    Restart(window, ini);
    Require(IsZoomed(window.hwnd) != FALSE, "maximized state");
    ShowWindow(window.hwnd, SW_RESTORE); CheckBounds(window.hwnd, normal);
    std::cout << "PASS: maximized state and restored bounds\n";

    ShowWindow(window.hwnd, SW_MINIMIZE);
    Restart(window, ini);
    Require(!IsZoomed(window.hwnd), "minimized normal must restore normal");
    CheckBounds(window.hwnd, normal);
    std::cout << "PASS: minimize normal then restart\n";

    ShowWindow(window.hwnd, SW_SHOWMAXIMIZED);
    ShowWindow(window.hwnd, SW_MINIMIZE);
    Restart(window, ini, true);
    Require(IsZoomed(window.hwnd) != FALSE, "minimized maximized must restore maximized");
    ShowWindow(window.hwnd, SW_RESTORE); CheckBounds(window.hwnd, normal);
    std::cout << "PASS: minimize maximized then restart\n";

    WritePrivateProfileStringW(L"output", L"suffix", L"_kept", ini.path);
    Restart(window, ini);
    wchar_t suffix[32]{};
    GetPrivateProfileStringW(L"output", L"suffix", L"", suffix, 32, ini.path);
    Require(std::wstring(suffix) == L"_kept", "suffix was overwritten");
    std::cout << "PASS: existing suffix survives geometry writes\n";

    WritePrivateProfileStringW(L"window", L"placement", L"broken-record", ini.path);
    Require(!window_placement::Restore(window.hwnd, ini.path), "corrupt record accepted");
    std::cout << "PASS: corrupt record falls back to defaults\n";

    auto invalid = Placement(window.hwnd);
    invalid.rcNormalPosition.right = invalid.rcNormalPosition.left - 1;
    WritePrivateProfileStructW(L"window", L"placement", &invalid, sizeof(invalid), ini.path);
    Require(!window_placement::Restore(window.hwnd, ini.path), "invalid rectangle accepted");
    std::cout << "PASS: negative window size rejected\n";

    auto offscreen = Placement(window.hwnd);
    offscreen.showCmd = SW_SHOWNORMAL;
    offscreen.rcNormalPosition = {-100'000, -100'000, -99'460, -99'620};
    Require(window_placement::IsValid(offscreen), "negative monitor coordinates rejected");
    WritePrivateProfileStructW(L"window", L"placement", &offscreen, sizeof(offscreen), ini.path);
    window.Reset();
    Require(window_placement::Restore(window.hwnd, ini.path), "offscreen restore failed");
    Require(MonitorFromWindow(window.hwnd, MONITOR_DEFAULTTONULL) != nullptr,
            "restored window remains completely offscreen");
    std::cout << "PASS: disconnected monitor position becomes visible\n";
    std::cout << "9 window-placement scenarios passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
