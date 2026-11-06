#include <fstream>
#include <iostream>

// Exercise the actual queue and WIC worker without opening the application
// window or loading/saving the user's settings.
#include "../src/main.cpp"

namespace {
void Require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}

std::vector<std::pair<size_t, size_t>> progress;
bool invalidItemPayload = false;
LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_APP_ITEM && lp) invalidItemPayload = true;
  if (message == WM_APP_PROGRESS) progress.emplace_back(static_cast<size_t>(wp), static_cast<size_t>(lp));
  return DefWindowProcW(hwnd, message, wp, lp);
}

void DrainMessages() {
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
}

struct Fixture {
  fs::path root = fs::temp_directory_path() /
    (L"mknk-queue-tests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));

  Fixture() { Require(fs::create_directory(root), "create isolated fixture directory"); }
  ~Fixture() { std::error_code ec; fs::remove_all(root, ec); }

  fs::path Bitmap(const wchar_t* name, int width = 160, int height = 120) {
    const auto path = root / name;
    Require(width % 4 == 0, "BMP fixture width must keep rows aligned");
    std::vector<BYTE> pixels(width * height * 3);
    unsigned seed = 12345;
    for (auto& value : pixels) { seed = seed * 1664525u + 1013904223u; value = static_cast<BYTE>(seed >> 24); }
    BITMAPFILEHEADER file{}; file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    BITMAPINFOHEADER info{}; info.biSize = sizeof(info); info.biWidth = width; info.biHeight = height;
    info.biPlanes = 1; info.biBitCount = 24; info.biSizeImage = static_cast<DWORD>(pixels.size());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(&file), sizeof(file));
    stream.write(reinterpret_cast<const char*>(&info), sizeof(info));
    stream.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    Require(stream.good(), "write BMP fixture");
    return path;
  }

  Settings Reset(const wchar_t* name) {
    DrainMessages(); progress.clear(); invalidItemPayload = false;
    { std::lock_guard lock(g_itemsMutex); g_items.clear(); }
    g_cancel = false;
    Settings settings; settings.outputDir = (root / name).wstring();
    fs::create_directory(settings.outputDir);
    return settings;
  }
};

size_t OutputCount(const Settings& settings) {
  return static_cast<size_t>(std::distance(fs::directory_iterator(settings.outputDir), fs::directory_iterator{}));
}

void Run(const Settings& settings) {
  Worker(settings, PrepareCompressionBatch()); DrainMessages();
  Require(!invalidItemPayload, "item notifications must not pack progress in LPARAM");
}

QueueItem Snapshot(size_t index) {
  std::lock_guard lock(g_itemsMutex);
  Require(index < g_items.size(), "expected queue entry");
  return *g_items[index];
}

size_t QueueSize() { std::lock_guard lock(g_itemsMutex); return g_items.size(); }

void RequireOutput(size_t index) {
  const auto item = Snapshot(index);
  Require(item.completed && !item.output.empty() && fs::is_regular_file(item.output), "expected successful JPEG output");
  Require(item.outputSize == fs::file_size(item.output), "output size must match the saved JPEG");
}

void WriteCorrupt(const fs::path& path, size_t bytes) {
  std::ofstream stream(path, std::ios::binary);
  const std::vector<char> invalid(bytes, 0);
  stream.write(invalid.data(), static_cast<std::streamsize>(invalid.size()));
  Require(stream.good(), "write corrupt input fixture");
}

void WriteSmallJpeg(const fs::path& bitmap, const fs::path& jpeg) {
  Require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "initialize JPEG fixture COM apartment");
  {
    ComPtr<IWICImagingFactory> factory;
    Require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))), "create JPEG fixture factory");
    ComPtr<IWICBitmapFrameDecode> source; ComPtr<IWICBitmapSource> pixels; UINT width{}, height{};
    Require(SUCCEEDED(LoadFrame(factory.Get(), bitmap, source)), "load JPEG fixture pixels");
    Require(SUCCEEDED(PreparePixels(factory.Get(), source.Get(), pixels, width, height)), "prepare JPEG fixture pixels");
    Require(SUCCEEDED(EncodeJpeg(factory.Get(), pixels.Get(), source.Get(), jpeg, 20, 1, false)), "encode small JPEG fixture");
  }
  CoUninitialize();
}
}

int main() {
  try {
    // Worker owns its COM apartment. Keeping COM initialized here would hide
    // releasing its WIC factory after CoUninitialize on a fresh worker thread.
    WNDCLASSW wc{}; wc.lpfnWndProc = TestWindowProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"mknkQueueTest";
    Require(RegisterClassW(&wc) != 0, "register message-only test window");
    g_window = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    Require(g_window != nullptr, "create message-only test window");
    Fixture fixture;
    const auto a = fixture.Bitmap(L"a.bmp"), b = fixture.Bitmap(L"b.bmp");

    auto settings = fixture.Reset(L"append");
    AddPath(a); Run(settings); RequireOutput(0);
    const auto firstItem = Snapshot(0);
    const auto firstOutput = firstItem.output, firstState = firstItem.state;
    const auto firstSize = firstItem.outputSize;
    const auto firstWriteTime = fs::last_write_time(firstOutput);
    progress.clear(); AddPath(b); Run(settings); RequireOutput(1);
    Require(Snapshot(0).output == firstOutput, "completed item was compressed again after adding a file");
    Require(Snapshot(0).state == firstState && Snapshot(0).outputSize == firstSize, "completed result changed");
    Require(fs::last_write_time(firstOutput) == firstWriteTime && OutputCount(settings) == 2, "completed output was rewritten or duplicated");
    Require(progress == std::vector<std::pair<size_t, size_t>>{{1, 1}}, "progress must count only the new batch");
    std::cout << "PASS: appending a file preserves completed results and counts only pending work\n";

    progress.clear(); Run(settings);
    Require(OutputCount(settings) == 2 && progress.empty(), "all-completed queue must perform no compression");
    std::cout << "PASS: all-completed queue creates no extra output\n";

    const auto compressed = fixture.root / L"already_yahoo.jpg";
    fs::copy_file(firstOutput, compressed);
    settings = fixture.Reset(L"compressed-input");
    fs::copy_file(firstOutput, fs::path(settings.outputDir) / L"already_yahoo_yahoo.jpg");
    AddPath(compressed); Run(settings); RequireOutput(0);
    Require(OutputCount(settings) == 2 && Snapshot(0).output != compressed.wstring(), "newly added compressed JPEG was skipped");
    std::cout << "PASS: newly added compressed JPEG is processed despite suffix and existing output\n";

    settings = fixture.Reset(L"readd");
    AddPath(a); AddPath(a);
    Require(QueueSize() == 1, "pending duplicate must remain a single queue entry");
    Run(settings); RequireOutput(0);
    const auto beforeReadd = Snapshot(0).output;
    auto samePath = a.wstring(); std::transform(samePath.begin(), samePath.end(), samePath.begin(), towupper);
    AddPath(samePath);
    Require(QueueSize() == 1 && Snapshot(0).output.empty() && Snapshot(0).state == L"待機中", "explicitly re-added completed path must become pending");
    Run(settings); RequireOutput(0);
    Require(Snapshot(0).output != beforeReadd && OutputCount(settings) == 2, "re-added completed path was not compressed again");
    std::cout << "PASS: pending duplicates are ignored and completed same-path additions are processed\n";

    settings = fixture.Reset(L"retry-grown-input");
    const auto broken = fixture.root / L"broken.bmp";
    WriteCorrupt(broken, 1);
    AddPath(broken); Run(settings);
    Require(!Snapshot(0).completed && Snapshot(0).output.empty() && OutputCount(settings) == 0, "invalid image must not count as completed");
    Require(Snapshot(0).inputSize == 1, "initial corrupt input size");
    fs::copy_file(a, broken, fs::copy_options::overwrite_existing);
    // Retry without adding the path again: the worker must re-stat the input.
    Run(settings); RequireOutput(0);
    Require(Snapshot(0).inputSize == fs::file_size(broken), "retry must refresh a repaired input that grew");
    Require(Snapshot(0).outputSize <= fs::file_size(broken), "grown repaired input output exceeds its current size");
    std::cout << "PASS: retry refreshes size and succeeds after a small corrupt input is repaired\n";

    settings = fixture.Reset(L"retry-shrunk-input");
    const auto shrunk = fixture.root / L"shrunk.jpg";
    WriteCorrupt(shrunk, 200000); AddPath(shrunk); Run(settings);
    Require(!Snapshot(0).completed && OutputCount(settings) == 0, "large corrupt JPEG must fail");
    fs::remove(shrunk); WriteSmallJpeg(a, shrunk);
    const auto actualSize = fs::file_size(shrunk);
    Require(actualSize < 200000, "JPEG fixture must shrink the previous input");
    AddPath(shrunk); Run(settings);
    const auto retried = Snapshot(0);
    Require(retried.inputSize == actualSize, "retry must refresh a repaired input that shrank");
    Require(!retried.completed || retried.outputSize <= actualSize, "retry must not create a JPEG larger than the current input");
    for (const auto& output : fs::directory_iterator(settings.outputDir))
      Require(output.file_size() <= actualSize, "retry left an oversized JPEG in the output directory");
    Require(retried.completed || (retried.output.empty() && retried.outputSize == 0), "failed retry must not retain a success result");
    std::cout << "PASS: retry never exceeds the current size after a large corrupt input becomes a small JPEG\n";

    settings = fixture.Reset(L"deleted-input");
    const auto deleted = fixture.Bitmap(L"deleted.bmp");
    AddPath(deleted); fs::remove(deleted); Run(settings);
    Require(!Snapshot(0).completed && Snapshot(0).output.empty() && Snapshot(0).state.starts_with(L"失敗"), "deleted input must report failure and remain retryable");
    Require(OutputCount(settings) == 0, "deleted input must not create an output");
    std::cout << "PASS: an input deleted after addition fails without an output\n";

    settings = fixture.Reset(L"cancel");
    AddPath(a); g_cancel = true; Run(settings);
    Require(Snapshot(0).output.empty() && OutputCount(settings) == 0, "cancelled batch must leave pending input untouched");
    g_cancel = false; Run(settings); RequireOutput(0);
    std::cout << "PASS: cancelled pending input can be retried\n";

    { std::lock_guard lock(g_itemsMutex); g_items.clear(); }
    AddPath(a); Run(settings); RequireOutput(0);
    Require(OutputCount(settings) == 2, "clearing the list must allow the same file to be compressed anew");
    std::cout << "PASS: clearing the list removes completion history\n";
    { std::lock_guard lock(g_itemsMutex); g_items.clear(); }
    DestroyWindow(g_window); g_window = nullptr;
    std::cout << "9 compression-queue scenarios passed (Worker owns COM lifetime)\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
