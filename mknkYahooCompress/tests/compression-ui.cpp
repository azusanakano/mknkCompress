#include <windows.h>
#include <shlobj.h>
#include <string>

// Redirect the application's only settings-folder lookup, keeping all real
// controls, WindowProc handlers, and asynchronous worker behavior intact.
static std::wstring testAppData;
static HRESULT WINAPI TestSettingsFolder(HWND, int, HANDLE, DWORD, LPWSTR destination) {
  return wcscpy_s(destination, MAX_PATH, testAppData.c_str()) == 0 ? S_OK : E_FAIL;
}
static BOOL WINAPI TestPostMessage(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
#define SHGetFolderPathW TestSettingsFolder
#define PostMessageW TestPostMessage
#define main QueueScenariosNotRunHere
#include "compression-queue.cpp"
#undef main
#undef PostMessageW
#undef SHGetFolderPathW

namespace {
HANDLE candidateReleaseEvent{};
std::atomic_bool pauseSecondCandidate{false}, enteredCandidateWait{false}, candidateWaitTimedOut{false};
bool cancelDuringSecond = false;
bool sawSecondCandidate = false;
bool finishedWhileJoinable = false;
size_t finishedMessages = 0;
std::optional<QueueItem> firstAtCancellation, thirdAtCancellation;

LRESULT CALLBACK IntegrationWindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_APP_ITEM) {
    if (lp != 0) invalidItemPayload = true;
    // The real worker publishes snapshots under this mutex. Reading the live
    // state without the same mutex would put a data race in the regression test.
    const auto item = Snapshot(static_cast<size_t>(wp));
    if (cancelDuringSecond && wp == 1 && item.state.starts_with(L"候補を比較中")) {
      firstAtCancellation = Snapshot(0);
      thirdAtCancellation = Snapshot(2);
      sawSecondCandidate = true;
      cancelDuringSecond = false;
      SendMessageW(hwnd, WM_COMMAND, IDC_CANCEL, 0);
      SetEvent(candidateReleaseEvent);
    }
  }
  if (message == WM_APP_PROGRESS)
    progress.emplace_back(static_cast<size_t>(wp), static_cast<size_t>(lp));
  const auto result = WindowProc(hwnd, message, wp, lp);
  if (message == WM_APP_FINISHED) {
    ++finishedMessages;
    if (g_worker.joinable() || g_running) finishedWhileJoinable = true;
  }
  return result;
}

struct ComApartment {
  ComApartment() { Require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), "initialize GUI COM apartment"); }
  ~ComApartment() { CoUninitialize(); }
};

struct TestWindow {
  HWND hwnd{};
  TestWindow() {
    candidateReleaseEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Require(candidateReleaseEvent != nullptr, "create test scheduling event");
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_PROGRESS_CLASS};
    Require(InitCommonControlsEx(&controls) != FALSE, "initialize GUI controls");
    g_instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.hInstance = g_instance; wc.lpfnWndProc = IntegrationWindowProc;
    wc.lpszClassName = L"mknkCompressionIntegrationTest";
    Require(RegisterClassW(&wc) != 0, "register real-handler test window");
    hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
      HWND_MESSAGE, nullptr, g_instance, nullptr);
    Require(hwnd != nullptr && g_list && g_progress && g_status, "create real GUI controls");
  }
  ~TestWindow() {
    // A failed assertion must also release the worker before WM_DESTROY joins.
    if (candidateReleaseEvent) SetEvent(candidateReleaseEvent);
    if (hwnd) {
      g_cancel = true;
      DestroyWindow(hwnd); // The real destruction handler joins any worker.
      g_window = g_list = g_progress = g_status = nullptr;
    }
    if (candidateReleaseEvent) CloseHandle(candidateReleaseEvent);
    candidateReleaseEvent = nullptr;
  }
};

void Drop(HWND hwnd, const std::vector<fs::path>& paths) {
  size_t characters = 1;
  for (const auto& path : paths) characters += path.wstring().size() + 1;
  const auto memory = GlobalAlloc(GHND, sizeof(DROPFILES) + characters * sizeof(wchar_t));
  Require(memory != nullptr, "allocate file-drop payload");
  auto* drop = static_cast<DROPFILES*>(GlobalLock(memory));
  Require(drop != nullptr, "lock file-drop payload");
  drop->pFiles = sizeof(DROPFILES); drop->fWide = TRUE;
  auto* destination = reinterpret_cast<wchar_t*>(drop + 1);
  for (const auto& path : paths) {
    const auto name = path.wstring();
    destination = std::copy(name.begin(), name.end(), destination) + 1;
  }
  GlobalUnlock(memory);
  SendMessageW(hwnd, WM_DROPFILES, reinterpret_cast<WPARAM>(memory), 0);
}

void WaitUntilDone() {
  const auto deadline = GetTickCount64() + 30000;
  while (g_running && GetTickCount64() < deadline) {
    DrainMessages();
    if (g_running) MsgWaitForMultipleObjectsEx(0, nullptr, 20, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
  }
  Require(!g_running, "asynchronous compression timed out");
  DrainMessages();
  Require(!g_worker.joinable() && !finishedWhileJoinable, "finished GUI must join the worker before accepting more work");
  Require(!invalidItemPayload, "item notifications must not contain packed progress");
}

Settings ResetUi(Fixture& fixture, HWND hwnd, const wchar_t* outputName) {
  Require(!g_running && !g_worker.joinable(), "reset only an idle GUI");
  SendMessageW(hwnd, WM_COMMAND, IDC_CLEAR, 0);
  const auto settings = fixture.Reset(outputName);
  SetWindowTextW(GetDlgItem(hwnd, IDC_OUTPUT), settings.outputDir.c_str());
  cancelDuringSecond = sawSecondCandidate = finishedWhileJoinable = false;
  pauseSecondCandidate = enteredCandidateWait = candidateWaitTimedOut = false;
  ResetEvent(candidateReleaseEvent);
  firstAtCancellation.reset(); thirdAtCancellation.reset(); finishedMessages = 0;
  return settings;
}
}

static BOOL WINAPI TestPostMessage(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
  const auto posted = ::PostMessageW(hwnd, message, wp, lp);
  if (message == WM_APP_ITEM && wp == 1 && pauseSecondCandidate &&
      Snapshot(1).state.starts_with(L"候補を比較中")) {
    bool armed = true;
    if (pauseSecondCandidate.compare_exchange_strong(armed, false)) {
      // The message and published row are real. Pausing this caller only makes
      // the in-flight cancellation deterministic under arbitrary scheduling.
      enteredCandidateWait = true;
      if (WaitForSingleObject(candidateReleaseEvent, 10000) != WAIT_OBJECT_0)
        candidateWaitTimedOut = true;
    }
  }
  return posted;
}

int main() {
  try {
    Fixture fixture;
    testAppData = fixture.root.wstring();
    ComApartment apartment;
    TestWindow window;
    const auto a = fixture.Bitmap(L"a.bmp"), b = fixture.Bitmap(L"b.bmp");

    auto settings = ResetUi(fixture, window.hwnd, L"drop-sequence");
    Drop(window.hwnd, {a});
    Require(g_running, "dropping A must start asynchronous compression");
    Drop(window.hwnd, {b});
    const auto rejectedStatus = GetText(g_status);
    Require(QueueSize() == 1, "busy drop must not add an unprocessed row");
    Require(rejectedStatus.find(L"圧縮中のため追加できません") != std::wstring::npos,
      "busy drop must explicitly explain that it was rejected");
    Require(rejectedStatus.find(L"追加しました") == std::wstring::npos,
      "busy drop must not report a successful addition");
    WaitUntilDone(); RequireOutput(0);
    const auto originalA = Snapshot(0);
    const auto aWriteTime = fs::last_write_time(originalA.output);
    std::cout << "PASS: a busy drop is explicitly rejected while the original file completes\n";

    progress.clear(); Drop(window.hwnd, {b});
    Require(GetText(g_status).starts_with(L"1枚を追加しました"), "drop status must count this addition, not the whole queue");
    WaitUntilDone(); RequireOutput(1);
    const auto originalB = Snapshot(1);
    Require(Snapshot(0).output == originalA.output && Snapshot(0).state == originalA.state,
      "adding B must preserve A's completed result");
    Require(fs::last_write_time(originalA.output) == aWriteTime && OutputCount(settings) == 2,
      "adding B must not rewrite or duplicate A's output");
    Require(progress == std::vector<std::pair<size_t, size_t>>{{1, 1}}, "B must be a one-item batch");
    progress.clear(); const auto finishedBeforeEmpty = finishedMessages;
    SendMessageW(window.hwnd, WM_COMMAND, IDC_START, 0);
    Require(!g_running && !g_worker.joinable() && finishedMessages == finishedBeforeEmpty,
      "starting an all-completed queue must leave the GUI idle");
    Require(OutputCount(settings) == 2 && progress.empty(), "empty start must not create work or progress");
    std::cout << "PASS: adding B skips completed A, reports one addition, and an empty start stays idle\n";

    Drop(window.hwnd, {a}); WaitUntilDone(); RequireOutput(0);
    Require(QueueSize() == 2 && Snapshot(0).output != originalA.output && Snapshot(1).output == originalB.output,
      "explicitly dropping A again must reuse its row and preserve completed B");
    Drop(window.hwnd, {originalA.output}); WaitUntilDone(); RequireOutput(2);
    Require(QueueSize() == 3, "directly dropping an earlier output JPEG must add its own row");
    Require(fs::equivalent(Snapshot(2).input, originalA.output), "directly dropped output JPEG must be the new row's input");
    Require(OutputCount(settings) == 4, "directly dropping an earlier output JPEG must create a fourth output");
    std::cout << "PASS: completed paths can be re-added and prior output JPEGs are accepted directly\n";

    settings = ResetUi(fixture, window.hwnd, L"accepted-count");
    const auto unsupported = fixture.root / L"notes.txt";
    { std::ofstream file(unsupported); file << "not an image"; }
    Require(AddPath(a) == 1 && AddPath(a) == 0 && AddPath(unsupported) == 0,
      "accepted count must exclude pending duplicates and unsupported files");
    Drop(window.hwnd, {a, b, unsupported});
    Require(GetText(g_status).starts_with(L"1枚を追加しました"), "mixed drop must count only the newly accepted B");
    Require(AddPath(unsupported) == 0 && AddPath(a) == 0, "direct additions while busy must be rejected");
    WaitUntilDone(); RequireOutput(0); RequireOutput(1);
    Require(QueueSize() == 2 && OutputCount(settings) == 2,
      "mixed drop must run both pending images exactly once");
    Require(progress == std::vector<std::pair<size_t, size_t>>{{1, 2}, {2, 2}}, "mixed drop progress must count pending work");
    Drop(window.hwnd, {unsupported});
    Require(!g_running && GetText(g_status).find(L"追加しました") == std::wstring::npos,
      "a drop accepting no images must stay idle and not report success");
    std::cout << "PASS: duplicate and unsupported paths do not inflate the accepted-addition count\n";

    settings = ResetUi(fixture, window.hwnd, L"partial-cancel");
    const auto second = fixture.Bitmap(L"second.bmp", 640, 480);
    const auto third = fixture.Bitmap(L"third.bmp");
    Require(AddPath(a) == 1 && AddPath(second) == 1 && AddPath(third) == 1, "queue cancellation fixtures");
    cancelDuringSecond = true;
    pauseSecondCandidate = true;
    SendMessageW(window.hwnd, WM_COMMAND, IDC_START, 0);
    Require(g_running, "manual start must launch the partial-cancellation batch");
    WaitUntilDone();
    Require(sawSecondCandidate && firstAtCancellation && thirdAtCancellation,
      "cancellation must be triggered by an actual second-item candidate notification");
    Require(enteredCandidateWait && !candidateWaitTimedOut,
      "second-item candidate must wait for the real cancel handler without timing out");
    Require(firstAtCancellation->completed && thirdAtCancellation->state == L"待機中",
      "first item must already be completed while the third is still pending");
    RequireOutput(0);
    const auto completedFirst = Snapshot(0), cancelledSecond = Snapshot(1), pendingThird = Snapshot(2);
    const auto completedWriteTime = fs::last_write_time(completedFirst.output);
    Require(completedFirst.output == firstAtCancellation->output && completedFirst.state == firstAtCancellation->state,
      "partial cancellation must preserve the first completed result");
    Require(!cancelledSecond.completed && cancelledSecond.output.empty() && cancelledSecond.state == L"キャンセル",
      "the in-flight second item must cancel without a result");
    Require(!pendingThird.completed && pendingThird.output.empty() && pendingThird.state == L"待機中" && pendingThird.candidateCount == 0,
      "the untouched third item must remain pending");
    Require(OutputCount(settings) == 1, "partial cancellation must keep only the first output");
    Require(progress == std::vector<std::pair<size_t, size_t>>{{1, 3}, {2, 3}}, "cancelled batch progress must reflect the two attempted items");
    progress.clear();
    SendMessageW(window.hwnd, WM_COMMAND, IDC_START, 0);
    WaitUntilDone(); RequireOutput(1); RequireOutput(2);
    Require(Snapshot(0).output == completedFirst.output && Snapshot(0).state == completedFirst.state &&
      fs::last_write_time(completedFirst.output) == completedWriteTime && OutputCount(settings) == 3,
      "resume must preserve the first output and complete only the second and third");
    Require(progress == std::vector<std::pair<size_t, size_t>>{{1, 2}, {2, 2}}, "resume progress must count only the two remaining items");
    std::cout << "PASS: in-flight cancellation preserves completed and untouched rows, then resumes only pending work\n";

    SendMessageW(window.hwnd, WM_APP_PROGRESS, 32768, 65536);
    Require(GetText(g_status) == L"処理中: 32768 / 65536" && SendMessageW(g_progress, PBM_GETPOS, 0, 0) == 50,
      "progress at 65536 items must retain the full total and show 50 percent");
    SendMessageW(window.hwnd, WM_APP_PROGRESS, 65536, 65536);
    Require(GetText(g_status) == L"処理中: 65536 / 65536" && SendMessageW(g_progress, PBM_GETPOS, 0, 0) == 100,
      "progress at 65536 items must reach 100 percent without 16-bit truncation");
    SendMessageW(window.hwnd, WM_APP_PROGRESS, 0, 0);
    Require(SendMessageW(g_progress, PBM_GETPOS, 0, 0) == 0, "zero-total progress must reset safely");
    std::cout << "PASS: the real progress handler preserves totals at the 65536-item boundary\n";
    Require(fs::is_regular_file(fixture.root / L"mknkYahooCompress" / L"settings.ini"), "GUI settings must be written only inside the isolated fixture");
    std::cout << "6 real WindowProc compression scenarios passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
