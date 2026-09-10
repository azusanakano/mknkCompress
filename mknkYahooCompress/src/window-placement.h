#pragma once

#include <windows.h>
#include <cstdint>

namespace window_placement {

// WINDOWPLACEMENT uses workspace coordinates. Keep Get/SetWindowPlacement
// paired so a taskbar on the top/left does not move the window on every launch.
inline bool IsValid(const WINDOWPLACEMENT& placement) {
  const RECT& r = placement.rcNormalPosition;
  const auto width = static_cast<std::int64_t>(r.right) - r.left;
  const auto height = static_cast<std::int64_t>(r.bottom) - r.top;
  constexpr LONG limit = 1'000'000;
  return placement.length == sizeof(WINDOWPLACEMENT) &&
         r.left >= -limit && r.left <= limit &&
         r.top >= -limit && r.top <= limit &&
         r.right >= -limit && r.right <= limit &&
         r.bottom >= -limit && r.bottom <= limit &&
         width > 0 && width <= 65'535 && height > 0 && height <= 65'535 &&
         (placement.showCmd == SW_SHOWNORMAL || placement.showCmd == SW_SHOWMAXIMIZED);
}

inline bool Save(HWND hwnd, const wchar_t* ini, bool maximizedBeforeMinimize = false) {
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  if (!GetWindowPlacement(hwnd, &placement)) return false;

  // Never relaunch minimized. Retain the normal bounds and the last restored
  // state, including minimize-after-maximize and close from the taskbar.
  // GetWindowPlacement does not promise WPF_RESTORETOMAXIMIZED on read.
  // The caller tracks the last non-minimized WM_SIZE state instead.
  const bool maximized = placement.showCmd == SW_SHOWMAXIMIZED ||
                         (IsIconic(hwnd) && maximizedBeforeMinimize);
  placement.showCmd = maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
  placement.flags = 0;
  placement.ptMinPosition = {-1, -1};
  placement.ptMaxPosition = {-1, -1};
  if (!IsValid(placement)) return false;

  // The profile API stores the complete record together with a checksum.
  // Other settings/sections in the existing file are retained.
  return WritePrivateProfileStructW(L"window", L"placement", &placement,
                                   sizeof(placement), ini) != FALSE;
}

inline bool Restore(HWND hwnd, const wchar_t* ini) {
  WINDOWPLACEMENT placement{};
  if (!GetPrivateProfileStructW(L"window", L"placement", &placement,
                               sizeof(placement), ini) || !IsValid(placement)) return false;
  placement.flags = 0;
  placement.ptMinPosition = {-1, -1};
  placement.ptMaxPosition = {-1, -1};
  // Windows relocates a completely off-screen saved window when the display
  // resolution or monitor configuration has changed.
  return SetWindowPlacement(hwnd, &placement) != FALSE;
}

} // namespace window_placement
