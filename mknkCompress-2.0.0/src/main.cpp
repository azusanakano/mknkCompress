#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <ocidl.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../resources/resource.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {
constexpr wchar_t kClassName[] = L"mknkCompressNativeWindow";
constexpr wchar_t kVersion[] = L"2.0.0 Native";
constexpr UINT WM_APP_ITEM = WM_APP + 1;
constexpr UINT WM_APP_FINISHED = WM_APP + 2;
constexpr int IDC_ADD = 1001, IDC_CLEAR = 1002, IDC_START = 1003, IDC_CANCEL = 1004;
constexpr int IDC_LIST = 1010, IDC_QUALITY = 1020, IDC_QUALITY_VALUE = 1021;
constexpr int IDC_PRESET_BAL = 1030, IDC_PRESET_HQ = 1031, IDC_PRESET_SMALL = 1032, IDC_PRESET_TINY = 1033;
constexpr int IDC_RESIZE = 1040, IDC_WIDTH = 1041, IDC_HEIGHT = 1042;
constexpr int IDC_METADATA = 1050, IDC_SMALLER = 1051, IDC_OUTPUT = 1060, IDC_BROWSE = 1061;
constexpr int IDC_SUFFIX = 1070, IDC_THEME = 1080, IDC_LIGHT = 1081, IDC_PROGRESS = 1090, IDC_STATUS = 1091;

struct Settings {
  int quality = 82;
  bool resize = false;
  UINT maxWidth = 1920, maxHeight = 1080;
  bool preserveMetadata = false;
  bool onlyIfSmaller = true;
  bool preserveTimestamps = true;
  std::wstring outputDir;
  std::wstring suffix = L"_compressed";
  int visualTheme = 0; // 0=antigravity, 1=gravity
  bool light = false;
};

struct QueueItem {
  std::wstring input;
  std::wstring output;
  ULONGLONG inputSize = 0, outputSize = 0;
  int selectedQuality = 0;
  int candidateCount = 0;
  double score = 0;
  std::wstring state = L"待機中";
};

struct Sample { UINT width = 0, height = 0; std::vector<BYTE> bgr; };
struct Candidate {
  fs::path path;
  ULONGLONG size = 0;
  int quality = 0;
  int subsampling = 0;
  double score = 0;
};

HINSTANCE g_instance{};
HWND g_window{}, g_list{}, g_quality{}, g_qualityValue{}, g_progress{}, g_status{};
HFONT g_font{}, g_titleFont{};
Settings g_settings;
std::vector<std::unique_ptr<QueueItem>> g_items;
std::mutex g_itemsMutex;
std::atomic_bool g_cancel{false}, g_running{false};
std::jthread g_worker;

std::wstring FormatBytes(ULONGLONG value) {
  const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB"};
  double n = static_cast<double>(value); int u = 0;
  while (n >= 1024.0 && u < 3) { n /= 1024.0; ++u; }
  wchar_t text[64]{};
  swprintf_s(text, u == 0 ? L"%.0f %s" : L"%.1f %s", n, units[u]);
  return text;
}

std::wstring IniPath() {
  wchar_t base[MAX_PATH]{};
  SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, base);
  fs::path dir = fs::path(base) / L"mknkCompress";
  std::error_code ec; fs::create_directories(dir, ec);
  return (dir / L"settings.ini").wstring();
}

void LoadSettings() {
  const auto ini = IniPath(); wchar_t buf[1024]{};
  g_settings.quality = GetPrivateProfileIntW(L"compression", L"quality", 82, ini.c_str());
  g_settings.resize = GetPrivateProfileIntW(L"compression", L"resize", 0, ini.c_str()) != 0;
  g_settings.maxWidth = GetPrivateProfileIntW(L"compression", L"maxWidth", 1920, ini.c_str());
  g_settings.maxHeight = GetPrivateProfileIntW(L"compression", L"maxHeight", 1080, ini.c_str());
  g_settings.preserveMetadata = GetPrivateProfileIntW(L"compression", L"metadata", 0, ini.c_str()) != 0;
  g_settings.onlyIfSmaller = GetPrivateProfileIntW(L"compression", L"onlyIfSmaller", 1, ini.c_str()) != 0;
  g_settings.visualTheme = GetPrivateProfileIntW(L"appearance", L"visualTheme", 0, ini.c_str());
  g_settings.light = GetPrivateProfileIntW(L"appearance", L"light", 0, ini.c_str()) != 0;
  GetPrivateProfileStringW(L"output", L"directory", L"", buf, 1024, ini.c_str()); g_settings.outputDir = buf;
  GetPrivateProfileStringW(L"output", L"suffix", L"_compressed", buf, 1024, ini.c_str()); g_settings.suffix = buf;
}

void WriteInt(const wchar_t* section, const wchar_t* key, int value) {
  wchar_t text[32]{}; swprintf_s(text, L"%d", value);
  WritePrivateProfileStringW(section, key, text, IniPath().c_str());
}

void SaveSettings() {
  WriteInt(L"compression", L"quality", g_settings.quality);
  WriteInt(L"compression", L"resize", g_settings.resize);
  WriteInt(L"compression", L"maxWidth", static_cast<int>(g_settings.maxWidth));
  WriteInt(L"compression", L"maxHeight", static_cast<int>(g_settings.maxHeight));
  WriteInt(L"compression", L"metadata", g_settings.preserveMetadata);
  WriteInt(L"compression", L"onlyIfSmaller", g_settings.onlyIfSmaller);
  WriteInt(L"appearance", L"visualTheme", g_settings.visualTheme);
  WriteInt(L"appearance", L"light", g_settings.light);
  WritePrivateProfileStringW(L"output", L"directory", g_settings.outputDir.c_str(), IniPath().c_str());
  WritePrivateProfileStringW(L"output", L"suffix", g_settings.suffix.c_str(), IniPath().c_str());
}

COLORREF Accent() { return g_settings.visualTheme == 0 ? RGB(39, 210, 225) : RGB(145, 91, 255); }
COLORREF Background() { return g_settings.light ? RGB(239, 247, 251) : (g_settings.visualTheme == 0 ? RGB(7, 21, 35) : RGB(15, 8, 30)); }
COLORREF Panel() { return g_settings.light ? RGB(255, 255, 255) : (g_settings.visualTheme == 0 ? RGB(13, 38, 56) : RGB(28, 16, 50)); }
COLORREF TextColor() { return g_settings.light ? RGB(24, 39, 53) : RGB(235, 246, 250); }

void SetText(HWND control, const std::wstring& value) { SetWindowTextW(control, value.c_str()); }
std::wstring GetText(HWND control) {
  const int n = GetWindowTextLengthW(control); std::wstring s(static_cast<size_t>(n) + 1, L'\0');
  if (n) GetWindowTextW(control, s.data(), n + 1); s.resize(n); return s;
}

void UpdateQualityLabel() { SetText(g_qualityValue, std::to_wstring(g_settings.quality)); }

void RefreshListRow(size_t i) {
  if (!g_list || i >= g_items.size()) return;
  auto& item = *g_items[i];
  std::wstring filename = fs::path(item.input).filename().wstring();
  LVITEMW row{}; row.mask = LVIF_TEXT; row.iItem = static_cast<int>(i); row.iSubItem = 0;
  row.pszText = filename.data();
  if (ListView_GetItemCount(g_list) <= static_cast<int>(i)) ListView_InsertItem(g_list, &row);
  else ListView_SetItem(g_list, &row);
  auto original = FormatBytes(item.inputSize); ListView_SetItemText(g_list, static_cast<int>(i), 1, original.data());
  ListView_SetItemText(g_list, static_cast<int>(i), 2, item.state.data());
  std::wstring result = item.outputSize ? FormatBytes(item.outputSize) : L"—";
  if (item.inputSize && item.outputSize && item.outputSize < item.inputSize) {
    const double p = 100.0 * (item.inputSize - item.outputSize) / item.inputSize;
    wchar_t b[80]{}; swprintf_s(b, L"%s  (-%.1f%%)", result.c_str(), p); result = b;
  }
  ListView_SetItemText(g_list, static_cast<int>(i), 3, result.data());
}

bool IsSupportedExtension(const fs::path& p) {
  auto e = p.extension().wstring(); std::transform(e.begin(), e.end(), e.begin(), towlower);
  static const std::unordered_set<std::wstring> extensions{L".jpg",L".jpeg",L".png",L".bmp",L".tif",L".tiff",L".gif",L".webp",L".heic",L".heif",L".avif"};
  return extensions.contains(e);
}

void AddPath(const fs::path& p) {
  if (g_running) return;
  std::error_code ec;
  if (fs::is_directory(p, ec)) {
    for (fs::recursive_directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
      if (!ec && it->is_regular_file(ec) && IsSupportedExtension(it->path())) AddPath(it->path());
    }
    return;
  }
  if (!fs::is_regular_file(p, ec) || !IsSupportedExtension(p)) return;
  const auto canonical = fs::weakly_canonical(p, ec).wstring();
  for (const auto& existing : g_items) if (_wcsicmp(existing->input.c_str(), canonical.c_str()) == 0) return;
  auto item = std::make_unique<QueueItem>(); item->input = canonical; item->inputSize = fs::file_size(p, ec);
  g_items.push_back(std::move(item)); RefreshListRow(g_items.size() - 1);
}

HRESULT LoadFrame(IWICImagingFactory* factory, const fs::path& path, ComPtr<IWICBitmapFrameDecode>& frame) {
  ComPtr<IWICBitmapDecoder> decoder;
  HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
  if (FAILED(hr)) return hr;
  return decoder->GetFrame(0, &frame);
}

ComPtr<IWICBitmapSource> OrientSource(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame) {
  PROPVARIANT value; PropVariantInit(&value); UINT orientation = 1;
  ComPtr<IWICMetadataQueryReader> reader;
  if (SUCCEEDED(frame->GetMetadataQueryReader(&reader))) {
    if (FAILED(reader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &value)))
      reader->GetMetadataByName(L"/ifd/{ushort=274}", &value);
    if (value.vt == VT_UI2) orientation = value.uiVal;
  }
  PropVariantClear(&value);
  WICBitmapTransformOptions transform = WICBitmapTransformRotate0;
  switch (orientation) {
    case 2: transform = WICBitmapTransformFlipHorizontal; break;
    case 3: transform = WICBitmapTransformRotate180; break;
    case 4: transform = WICBitmapTransformFlipVertical; break;
    case 5: transform = static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal); break;
    case 6: transform = WICBitmapTransformRotate90; break;
    case 7: transform = static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal); break;
    case 8: transform = WICBitmapTransformRotate270; break;
    default: return frame;
  }
  ComPtr<IWICBitmapFlipRotator> rotator;
  if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rotator)) && SUCCEEDED(rotator->Initialize(frame, transform))) return rotator;
  return frame;
}

HRESULT PreparePixels(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame, const Settings& settings,
                      ComPtr<IWICBitmapSource>& output, UINT& width, UINT& height) {
  auto source = OrientSource(factory, frame);
  HRESULT hr = source->GetSize(&width, &height); if (FAILED(hr)) return hr;
  if (settings.resize && settings.maxWidth && settings.maxHeight && (width > settings.maxWidth || height > settings.maxHeight)) {
    const double factor = std::min(static_cast<double>(settings.maxWidth) / width, static_cast<double>(settings.maxHeight) / height);
    width = std::max(1u, static_cast<UINT>(std::floor(width * factor + 0.5)));
    height = std::max(1u, static_cast<UINT>(std::floor(height * factor + 0.5)));
    ComPtr<IWICBitmapScaler> scaler; hr = factory->CreateBitmapScaler(&scaler); if (FAILED(hr)) return hr;
    hr = scaler->Initialize(source.Get(), width, height, WICBitmapInterpolationModeFant); if (FAILED(hr)) return hr;
    source = scaler;
  }
  ComPtr<IWICFormatConverter> rgba; hr = factory->CreateFormatConverter(&rgba); if (FAILED(hr)) return hr;
  hr = rgba->Initialize(source.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) return hr;
  const UINT stride32 = width * 4; std::vector<BYTE> pixels(static_cast<size_t>(stride32) * height);
  hr = rgba->CopyPixels(nullptr, stride32, static_cast<UINT>(pixels.size()), pixels.data()); if (FAILED(hr)) return hr;
  std::vector<BYTE> bgr(static_cast<size_t>(width) * height * 3);
  for (size_t src = 0, dst = 0; src < pixels.size(); src += 4, dst += 3) {
    const UINT a = pixels[src + 3];
    bgr[dst] = static_cast<BYTE>((pixels[src] * a + 255u * (255u - a) + 127u) / 255u);
    bgr[dst + 1] = static_cast<BYTE>((pixels[src + 1] * a + 255u * (255u - a) + 127u) / 255u);
    bgr[dst + 2] = static_cast<BYTE>((pixels[src + 2] * a + 255u * (255u - a) + 127u) / 255u);
  }
  ComPtr<IWICBitmap> bitmap;
  hr = factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat24bppBGR, width * 3,
                                        static_cast<UINT>(bgr.size()), bgr.data(), &bitmap);
  if (SUCCEEDED(hr)) output = bitmap; return hr;
}

HRESULT MakeSample(IWICImagingFactory* factory, IWICBitmapSource* source, Sample& sample) {
  UINT w{}, h{}; HRESULT hr = source->GetSize(&w, &h); if (FAILED(hr)) return hr;
  const double scale = std::min(1.0, 320.0 / std::max(w, h));
  sample.width = std::max(1u, static_cast<UINT>(std::floor(w * scale + 0.5)));
  sample.height = std::max(1u, static_cast<UINT>(std::floor(h * scale + 0.5)));
  ComPtr<IWICBitmapSource> current = source;
  ComPtr<IWICBitmapScaler> scaler;
  if (sample.width != w || sample.height != h) {
    hr = factory->CreateBitmapScaler(&scaler); if (FAILED(hr)) return hr;
    hr = scaler->Initialize(source, sample.width, sample.height, WICBitmapInterpolationModeFant); if (FAILED(hr)) return hr;
    current = scaler;
  }
  ComPtr<IWICFormatConverter> converter; hr = factory->CreateFormatConverter(&converter); if (FAILED(hr)) return hr;
  hr = converter->Initialize(current.Get(), GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) return hr;
  sample.bgr.resize(static_cast<size_t>(sample.width) * sample.height * 3);
  return converter->CopyPixels(nullptr, sample.width * 3, static_cast<UINT>(sample.bgr.size()), sample.bgr.data());
}

double PerceptualScore(const Sample& left, const Sample& right) {
  if (left.width != right.width || left.height != right.height || left.bgr.size() != right.bgr.size()) return 0;
  double luma = 0, chroma = 0, edge = 0; size_t edgeCount = 0;
  const UINT w = left.width, h = left.height;
  auto yAt = [](const std::vector<BYTE>& p, size_t i) { return .299*p[i+2] + .587*p[i+1] + .114*p[i]; };
  for (UINT y = 0; y < h; ++y) for (UINT x = 0; x < w; ++x) {
    const size_t i = (static_cast<size_t>(y)*w+x)*3;
    const double lb=left.bgr[i], lg=left.bgr[i+1], lr=left.bgr[i+2];
    const double rb=right.bgr[i], rg=right.bgr[i+1], rr=right.bgr[i+2];
    const double ly=.299*lr+.587*lg+.114*lb, ry=.299*rr+.587*rg+.114*rb;
    const double lcb=-.168736*lr-.331264*lg+.5*lb, rcb=-.168736*rr-.331264*rg+.5*rb;
    const double lcr=.5*lr-.418688*lg-.081312*lb, rcr=.5*rr-.418688*rg-.081312*rb;
    luma += (ly-ry)*(ly-ry); chroma += ((lcb-rcb)*(lcb-rcb)+(lcr-rcr)*(lcr-rcr))/2;
    if (x) { const size_t p=i-3; const double d=(ly-yAt(left.bgr,p))-(ry-yAt(right.bgr,p)); edge+=d*d; ++edgeCount; }
    if (y) { const size_t p=i-static_cast<size_t>(w)*3; const double d=(ly-yAt(left.bgr,p))-(ry-yAt(right.bgr,p)); edge+=d*d; ++edgeCount; }
  }
  const double pixels = static_cast<double>(w)*h;
  const double error=(.68*std::sqrt(luma/pixels)+.20*std::sqrt(chroma/pixels)+.12*std::sqrt(edge/std::max<size_t>(1,edgeCount)))/255.;
  return std::clamp(1.-error, 0., 1.);
}

double ScoreFloor(int quality, double baseline) {
  const double tolerance = quality >= 92 ? .0020 : quality >= 82 ? .0032 : quality >= 70 ? .0048 : .0065;
  return std::max(.964 + quality * .00023, baseline - tolerance);
}

HRESULT CopyMetadata(IWICBitmapFrameDecode* source, IWICBitmapFrameEncode* destination) {
  ComPtr<IWICMetadataBlockReader> reader; ComPtr<IWICMetadataBlockWriter> writer;
  HRESULT hr = source->QueryInterface(IID_PPV_ARGS(&reader)); if (FAILED(hr)) return hr;
  hr = destination->QueryInterface(IID_PPV_ARGS(&writer)); if (FAILED(hr)) return hr;
  return writer->InitializeFromBlockReader(reader.Get());
}

HRESULT EncodeJpeg(IWICImagingFactory* factory, IWICBitmapSource* pixels, IWICBitmapFrameDecode* original,
                   const fs::path& path, int quality, int subsampling, bool metadata) {
  ComPtr<IWICStream> stream; HRESULT hr=factory->CreateStream(&stream); if (FAILED(hr)) return hr;
  hr=stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE); if (FAILED(hr)) return hr;
  ComPtr<IWICBitmapEncoder> encoder; hr=factory->CreateEncoder(GUID_ContainerFormatJpeg,nullptr,&encoder); if(FAILED(hr)) return hr;
  hr=encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache); if(FAILED(hr)) return hr;
  ComPtr<IWICBitmapFrameEncode> frame; ComPtr<IPropertyBag2> bag; hr=encoder->CreateNewFrame(&frame,&bag); if(FAILED(hr)) return hr;
  PROPBAG2 options[3]{}; VARIANT values[3]; for(auto& v:values) VariantInit(&v);
  options[0].pstrName=const_cast<wchar_t*>(L"ImageQuality"); values[0].vt=VT_R4; values[0].fltVal=quality/100.f;
  options[1].pstrName=const_cast<wchar_t*>(L"JpegYCrCbSubsampling"); values[1].vt=VT_UI1; values[1].bVal=static_cast<BYTE>(subsampling);
  options[2].pstrName=const_cast<wchar_t*>(L"InterlaceOption"); values[2].vt=VT_BOOL; values[2].boolVal=VARIANT_TRUE;
  if (bag) bag->Write(3,options,values);
  hr=frame->Initialize(bag.Get()); if(FAILED(hr)) return hr;
  UINT w{},h{}; pixels->GetSize(&w,&h); frame->SetSize(w,h);
  WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR; frame->SetPixelFormat(&format);
  if(metadata) CopyMetadata(original,frame.Get());
  hr=frame->WriteSource(pixels,nullptr); if(FAILED(hr)) return hr;
  hr=frame->Commit(); if(FAILED(hr)) return hr; return encoder->Commit();
}

std::vector<std::pair<int,int>> CandidateDefinitions(int quality) {
  std::vector<int> steps = quality>=90 ? std::vector<int>{0,3,6,9,12,16} : quality>=75 ? std::vector<int>{0,4,7,10,14,18} : std::vector<int>{0,5,9,13,17,22};
  std::vector<std::pair<int,int>> out;
  for(int step:steps) { int q=std::max(35,quality-step); if(q>=88) out.emplace_back(q,3); out.emplace_back(q,1); }
  return out;
}

fs::path UniqueOutput(const fs::path& input,const Settings& settings) {
  fs::path dir=settings.outputDir.empty()?input.parent_path():fs::path(settings.outputDir);
  std::error_code ec; fs::create_directories(dir,ec);
  for(int i=0;i<10000;++i) {
    std::wstring count=i?L" ("+std::to_wstring(i+1)+L")":L"";
    fs::path p=dir/(input.stem().wstring()+settings.suffix+count+L".jpg");
    if(!fs::exists(p,ec) && _wcsicmp(p.c_str(),input.c_str())!=0) return p;
  }
  throw std::runtime_error("output path");
}

std::wstring HResultText(HRESULT hr) {
  wchar_t* message=nullptr; FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,hr,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),reinterpret_cast<wchar_t*>(&message),0,nullptr);
  std::wstring result=message?message:L"画像コーデックで処理できません"; if(message)LocalFree(message); return result;
}

void CompressOne(size_t index, IWICImagingFactory* factory, const Settings& settings) {
  QueueItem* item=g_items[index].get(); item->state=L"解析中"; PostMessageW(g_window,WM_APP_ITEM,index,0);
  ComPtr<IWICBitmapFrameDecode> original; HRESULT hr=LoadFrame(factory,item->input,original);
  if(FAILED(hr)){item->state=L"失敗: "+HResultText(hr);PostMessageW(g_window,WM_APP_ITEM,index,0);return;}
  ComPtr<IWICBitmapSource> pixels; UINT w{},h{}; hr=PreparePixels(factory,original.Get(),settings,pixels,w,h);
  if(FAILED(hr)){item->state=L"失敗: デコード";PostMessageW(g_window,WM_APP_ITEM,index,0);return;}
  Sample reference; if(FAILED(MakeSample(factory,pixels.Get(),reference))){item->state=L"失敗: 画質解析";PostMessageW(g_window,WM_APP_ITEM,index,0);return;}
  const auto definitions=CandidateDefinitions(settings.quality); std::vector<Candidate> candidates;
  fs::path tempDir=fs::temp_directory_path()/L"mknkCompress"; std::error_code ec;fs::create_directories(tempDir,ec);
  for(size_t n=0;n<definitions.size()&&!g_cancel;++n){
    const auto [q,sub]=definitions[n]; fs::path temp=tempDir/(L"candidate-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(index)+L"-"+std::to_wstring(n)+L".jpg");
    fs::remove(temp,ec); hr=EncodeJpeg(factory,pixels.Get(),original.Get(),temp,q,sub,settings.preserveMetadata);
    if(FAILED(hr))continue;
    ComPtr<IWICBitmapFrameDecode> decoded; if(FAILED(LoadFrame(factory,temp,decoded))){fs::remove(temp,ec);continue;}
    ComPtr<IWICFormatConverter> cv; factory->CreateFormatConverter(&cv);
    if(!cv||FAILED(cv->Initialize(decoded.Get(),GUID_WICPixelFormat24bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){fs::remove(temp,ec);continue;}
    Sample sample;if(FAILED(MakeSample(factory,cv.Get(),sample))){fs::remove(temp,ec);continue;}
    candidates.push_back({temp,fs::file_size(temp,ec),q,sub,PerceptualScore(reference,sample)});
    item->state=L"候補を比較中 "+std::to_wstring(n+1)+L" / "+std::to_wstring(definitions.size());PostMessageW(g_window,WM_APP_ITEM,index,0);
  }
  if(g_cancel){item->state=L"キャンセル";for(auto& c:candidates)fs::remove(c.path,ec);PostMessageW(g_window,WM_APP_ITEM,index,0);return;}
  if(candidates.empty()){item->state=L"失敗: JPEGエンコード";PostMessageW(g_window,WM_APP_ITEM,index,0);return;}
  double baseline=0;for(const auto& c:candidates)if(c.quality==settings.quality)baseline=std::max(baseline,c.score);
  if(!baseline)for(const auto& c:candidates)baseline=std::max(baseline,c.score);
  const double floor=ScoreFloor(settings.quality,baseline);
  auto winner=candidates.end();
  for(auto it=candidates.begin();it!=candidates.end();++it)if(it->score>=floor&&(winner==candidates.end()||it->size<winner->size||(it->size==winner->size&&it->score>winner->score)))winner=it;
  if(winner==candidates.end())winner=std::max_element(candidates.begin(),candidates.end(),[](auto&a,auto&b){return a.score<b.score;});
  item->candidateCount=static_cast<int>(candidates.size());item->selectedQuality=winner->quality;item->score=winner->score;
  if(settings.onlyIfSmaller&&winner->size>=item->inputSize&&!settings.resize){
    item->outputSize=item->inputSize;item->state=L"保存なし（元画像以上）";
  }else{
    try{
      const auto output=UniqueOutput(item->input,settings);fs::rename(winner->path,output,ec);if(ec){fs::copy_file(winner->path,output,fs::copy_options::none,ec);if(!ec)fs::remove(winner->path,ec);}
      if(ec)throw std::runtime_error("move");
      if(settings.preserveTimestamps){auto time=fs::last_write_time(item->input,ec);if(!ec)fs::last_write_time(output,time,ec);}
      item->output=output.wstring();item->outputSize=winner->size;
      wchar_t state[128]{};swprintf_s(state,L"完了 Q%d / %.2f%%",winner->quality,winner->score*100);item->state=state;
      winner->path.clear();
    }catch(...){item->state=L"失敗: 保存できません";}
  }
  for(auto& c:candidates)if(!c.path.empty())fs::remove(c.path,ec);
  PostMessageW(g_window,WM_APP_ITEM,index,0);
}

void Worker(Settings settings) {
  CoInitializeEx(nullptr,COINIT_MULTITHREADED);ComPtr<IWICImagingFactory> factory;
  HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
  if(FAILED(hr)){PostMessageW(g_window,WM_APP_FINISHED,1,0);CoUninitialize();return;}
  for(size_t i=0;i<g_items.size()&&!g_cancel;++i){CompressOne(i,factory.Get(),settings);PostMessageW(g_window,WM_APP_ITEM,i,MAKELPARAM(i+1,g_items.size()));}
  PostMessageW(g_window,WM_APP_FINISHED,0,0);CoUninitialize();
}

void CollectSettings(HWND hwnd) {
  g_settings.quality=static_cast<int>(SendMessageW(g_quality,TBM_GETPOS,0,0));
  g_settings.resize=Button_GetCheck(GetDlgItem(hwnd,IDC_RESIZE))==BST_CHECKED;
  g_settings.preserveMetadata=Button_GetCheck(GetDlgItem(hwnd,IDC_METADATA))==BST_CHECKED;
  g_settings.onlyIfSmaller=Button_GetCheck(GetDlgItem(hwnd,IDC_SMALLER))==BST_CHECKED;
  g_settings.light=Button_GetCheck(GetDlgItem(hwnd,IDC_LIGHT))==BST_CHECKED;
  g_settings.visualTheme=static_cast<int>(SendMessageW(GetDlgItem(hwnd,IDC_THEME),CB_GETCURSEL,0,0));
  g_settings.maxWidth=std::clamp(_wtoi(GetText(GetDlgItem(hwnd,IDC_WIDTH)).c_str()),1,32768);
  g_settings.maxHeight=std::clamp(_wtoi(GetText(GetDlgItem(hwnd,IDC_HEIGHT)).c_str()),1,32768);
  g_settings.outputDir=GetText(GetDlgItem(hwnd,IDC_OUTPUT));g_settings.suffix=GetText(GetDlgItem(hwnd,IDC_SUFFIX));
  if(g_settings.suffix.empty())g_settings.suffix=L"_compressed";SaveSettings();
}

void ChooseFiles(HWND hwnd) {
  ComPtr<IFileOpenDialog> dialog;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))return;
  dialog->SetOptions(FOS_ALLOWMULTISELECT|FOS_FORCEFILESYSTEM|FOS_FILEMUSTEXIST);
  COMDLG_FILTERSPEC filters[]={{L"画像",L"*.jpg;*.jpeg;*.png;*.bmp;*.tif;*.tiff;*.gif;*.webp;*.heic;*.heif;*.avif"},{L"すべて",L"*.*"}};dialog->SetFileTypes(2,filters);
  if(SUCCEEDED(dialog->Show(hwnd))){ComPtr<IShellItemArray> results;if(SUCCEEDED(dialog->GetResults(&results))){DWORD count{};results->GetCount(&count);for(DWORD i=0;i<count;++i){ComPtr<IShellItem> item;PWSTR path{};if(SUCCEEDED(results->GetItemAt(i,&item))&&SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){AddPath(path);CoTaskMemFree(path);}}}}
}

void ChooseFolder(HWND hwnd) {
  ComPtr<IFileOpenDialog> dialog;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))return;
  dialog->SetOptions(FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
  if(SUCCEEDED(dialog->Show(hwnd))){ComPtr<IShellItem> item;PWSTR path{};if(SUCCEEDED(dialog->GetResult(&item))&&SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){SetText(GetDlgItem(hwnd,IDC_OUTPUT),path);CoTaskMemFree(path);}}
}

HWND Make(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h) {
  HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),g_instance,nullptr);
  SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);return c;
}

void Layout(HWND hwnd,int width,int height) {
  const int pad=24, right=350, gap=18, top=108, bottom=62;
  MoveWindow(g_list,pad,top,width-right-pad-gap,height-top-bottom,TRUE);
  int x=width-right,y=top;
  auto M=[&](int id,int yy,int hh){MoveWindow(GetDlgItem(hwnd,id),x,yy,right-pad,hh,TRUE);};
  M(IDC_THEME,y,30);M(IDC_LIGHT,y+36,26);y+=78;
  int bw=(right-pad-12)/2;MoveWindow(GetDlgItem(hwnd,IDC_PRESET_BAL),x,y,bw,32,TRUE);MoveWindow(GetDlgItem(hwnd,IDC_PRESET_HQ),x+bw+12,y,bw,32,TRUE);
  MoveWindow(GetDlgItem(hwnd,IDC_PRESET_SMALL),x,y+40,bw,32,TRUE);MoveWindow(GetDlgItem(hwnd,IDC_PRESET_TINY),x+bw+12,y+40,bw,32,TRUE);y+=96;
  M(IDC_QUALITY,y,30);MoveWindow(g_qualityValue,x+right-pad-46,y-22,46,22,TRUE);y+=42;
  M(IDC_RESIZE,y,25);MoveWindow(GetDlgItem(hwnd,IDC_WIDTH),x,y+30,bw,27,TRUE);MoveWindow(GetDlgItem(hwnd,IDC_HEIGHT),x+bw+12,y+30,bw,27,TRUE);y+=69;
  M(IDC_METADATA,y,25);M(IDC_SMALLER,y+28,25);y+=64;
  MoveWindow(GetDlgItem(hwnd,IDC_OUTPUT),x,y,right-pad-76,28,TRUE);MoveWindow(GetDlgItem(hwnd,IDC_BROWSE),x+right-pad-68,y,68,28,TRUE);y+=38;
  M(IDC_SUFFIX,y,28);
  MoveWindow(g_progress,pad,height-48,width-right-pad-gap,16,TRUE);MoveWindow(g_status,pad,height-29,width-right-pad-gap,22,TRUE);
  MoveWindow(GetDlgItem(hwnd,IDC_START),x,height-58,right-pad,38,TRUE);MoveWindow(GetDlgItem(hwnd,IDC_CANCEL),x,height-58,right-pad,38,TRUE);
}

void CreateControls(HWND hwnd) {
  g_font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  g_titleFont=CreateFontW(-31,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  auto title=Make(hwnd,L"STATIC",L"mknkCompress",SS_LEFT,0,24,20,280,42);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(g_titleFont),TRUE);
  Make(hwnd,L"STATIC",L"2.0.0 NATIVE  •  完全オフライン  •  JPG専用",SS_LEFT,0,27,63,430,25);
  Make(hwnd,L"BUTTON",L"画像を追加",BS_PUSHBUTTON,IDC_ADD,470,30,120,36);Make(hwnd,L"BUTTON",L"一覧を消去",BS_PUSHBUTTON,IDC_CLEAR,600,30,120,36);
  g_list=Make(hwnd,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS|WS_BORDER,IDC_LIST,24,108,600,500);
  ListView_SetExtendedListViewStyle(g_list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
  LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;const wchar_t* names[]={L"ファイル",L"元サイズ",L"状態",L"圧縮後"};int widths[]={260,90,210,130};for(int i=0;i<4;++i){col.pszText=const_cast<wchar_t*>(names[i]);col.cx=widths[i];ListView_InsertColumn(g_list,i,&col);}
  auto theme=Make(hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST,IDC_THEME,0,0,100,100);SendMessageW(theme,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"反重力斥力場"));SendMessageW(theme,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"重力圧縮"));SendMessageW(theme,CB_SETCURSEL,g_settings.visualTheme,0);
  auto light=Make(hwnd,L"BUTTON",L"ライト表示",BS_AUTOCHECKBOX,IDC_LIGHT,0,0,100,24);Button_SetCheck(light,g_settings.light?BST_CHECKED:BST_UNCHECKED);
  Make(hwnd,L"BUTTON",L"バランス Q82",BS_PUSHBUTTON,IDC_PRESET_BAL,0,0,100,30);Make(hwnd,L"BUTTON",L"最高画質 Q92",BS_PUSHBUTTON,IDC_PRESET_HQ,0,0,100,30);
  Make(hwnd,L"BUTTON",L"最小サイズ Q70",BS_PUSHBUTTON,IDC_PRESET_SMALL,0,0,100,30);Make(hwnd,L"BUTTON",L"しっかり圧縮 Q62",BS_PUSHBUTTON,IDC_PRESET_TINY,0,0,100,30);
  g_quality=Make(hwnd,TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_AUTOTICKS,IDC_QUALITY,0,0,100,30);SendMessageW(g_quality,TBM_SETRANGE,TRUE,MAKELPARAM(35,100));SendMessageW(g_quality,TBM_SETPOS,TRUE,g_settings.quality);
  g_qualityValue=Make(hwnd,L"STATIC",L"82",SS_RIGHT,IDC_QUALITY_VALUE,0,0,40,22);UpdateQualityLabel();
  auto resize=Make(hwnd,L"BUTTON",L"画像サイズを変更（縦横比を維持）",BS_AUTOCHECKBOX,IDC_RESIZE,0,0,100,25);Button_SetCheck(resize,g_settings.resize?BST_CHECKED:BST_UNCHECKED);
  Make(hwnd,L"EDIT",std::to_wstring(g_settings.maxWidth).c_str(),ES_NUMBER|WS_BORDER,IDC_WIDTH,0,0,100,27);Make(hwnd,L"EDIT",std::to_wstring(g_settings.maxHeight).c_str(),ES_NUMBER|WS_BORDER,IDC_HEIGHT,0,0,100,27);
  auto metadata=Make(hwnd,L"BUTTON",L"撮影情報・ICCを保持",BS_AUTOCHECKBOX,IDC_METADATA,0,0,100,25);Button_SetCheck(metadata,g_settings.preserveMetadata?BST_CHECKED:BST_UNCHECKED);
  auto smaller=Make(hwnd,L"BUTTON",L"小さくなる場合だけ保存",BS_AUTOCHECKBOX,IDC_SMALLER,0,0,100,25);Button_SetCheck(smaller,g_settings.onlyIfSmaller?BST_CHECKED:BST_UNCHECKED);
  Make(hwnd,L"EDIT",g_settings.outputDir.c_str(),ES_AUTOHSCROLL|WS_BORDER,IDC_OUTPUT,0,0,100,28);Make(hwnd,L"BUTTON",L"参照",BS_PUSHBUTTON,IDC_BROWSE,0,0,60,28);
  Make(hwnd,L"EDIT",g_settings.suffix.c_str(),ES_AUTOHSCROLL|WS_BORDER,IDC_SUFFIX,0,0,100,28);
  g_progress=Make(hwnd,PROGRESS_CLASSW,L"",PBS_SMOOTH,IDC_PROGRESS,0,0,100,16);SendMessageW(g_progress,PBM_SETRANGE32,0,100);
  g_status=Make(hwnd,L"STATIC",L"画像をドロップするか［画像を追加］を押してください",SS_LEFT,IDC_STATUS,0,0,100,22);
  Make(hwnd,L"BUTTON",L"画像を圧縮",BS_DEFPUSHBUTTON,IDC_START,0,0,100,38);auto cancel=Make(hwnd,L"BUTTON",L"キャンセル",BS_PUSHBUTTON,IDC_CANCEL,0,0,100,38);ShowWindow(cancel,SW_HIDE);
}

void StartCompression(HWND hwnd) {
  if (g_running || g_items.empty()) return;
  CollectSettings(hwnd);
  g_cancel=false;g_running=true;
  for(size_t i=0;i<g_items.size();++i){g_items[i]->state=L"待機中";g_items[i]->output.clear();g_items[i]->outputSize=0;RefreshListRow(i);}
  ShowWindow(GetDlgItem(hwnd,IDC_START),SW_HIDE);
  ShowWindow(GetDlgItem(hwnd,IDC_CANCEL),SW_SHOW);
  g_worker=std::jthread(Worker,g_settings);
}

LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg){
    case WM_CREATE:{g_window=hwnd;LoadSettings();CreateControls(hwnd);DragAcceptFiles(hwnd,TRUE);BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));return 0;}
    case WM_SIZE:Layout(hwnd,LOWORD(lp),HIWORD(lp));return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);HBRUSH bg=CreateSolidBrush(Background());FillRect(dc,&r,bg);DeleteObject(bg);HPEN pen=CreatePen(PS_SOLID,2,Accent());auto old=SelectObject(dc,pen);MoveToEx(dc,24,94,nullptr);LineTo(dc,r.right-24,94);SelectObject(dc,old);DeleteObject(pen);EndPaint(hwnd,&ps);return 0;}
    case WM_CTLCOLORSTATIC:{HDC dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,TextColor());SetBkColor(dc,Background());static HBRUSH brush{};if(brush)DeleteObject(brush);brush=CreateSolidBrush(Background());return reinterpret_cast<LRESULT>(brush);}
    case WM_HSCROLL:if(reinterpret_cast<HWND>(lp)==g_quality){g_settings.quality=static_cast<int>(SendMessageW(g_quality,TBM_GETPOS,0,0));UpdateQualityLabel();}return 0;
    case WM_DROPFILES:{HDROP drop=reinterpret_cast<HDROP>(wp);UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<count;++i){UINT n=DragQueryFileW(drop,i,nullptr,0);std::wstring path(static_cast<size_t>(n)+1,L'\0');DragQueryFileW(drop,i,path.data(),n+1);path.resize(n);AddPath(path);}DragFinish(drop);SetText(g_status,std::to_wstring(g_items.size())+L"枚を追加しました。自動圧縮を開始します…");StartCompression(hwnd);return 0;}
    case WM_COMMAND:{const int id=LOWORD(wp);
      if(id==IDC_ADD)ChooseFiles(hwnd);
      else if(id==IDC_CLEAR&&!g_running){g_items.clear();ListView_DeleteAllItems(g_list);SetText(g_status,L"一覧を消去しました");}
      else if(id==IDC_BROWSE)ChooseFolder(hwnd);
      else if(id>=IDC_PRESET_BAL&&id<=IDC_PRESET_TINY){int q=id==IDC_PRESET_BAL?82:id==IDC_PRESET_HQ?92:id==IDC_PRESET_SMALL?70:62;SendMessageW(g_quality,TBM_SETPOS,TRUE,q);g_settings.quality=q;UpdateQualityLabel();}
      else if(id==IDC_THEME||id==IDC_LIGHT){CollectSettings(hwnd);InvalidateRect(hwnd,nullptr,TRUE);}
      else if(id==IDC_START){StartCompression(hwnd);}
      else if(id==IDC_CANCEL&&g_running){g_cancel=true;SetText(g_status,L"現在の候補処理が終わり次第キャンセルします…");}
      return 0;}
    case WM_APP_ITEM:{size_t i=static_cast<size_t>(wp);RefreshListRow(i);if(lp){const int done=LOWORD(lp),total=HIWORD(lp);SendMessageW(g_progress,PBM_SETPOS,total?done*100/total:0,0);SetText(g_status,L"処理中: "+std::to_wstring(done)+L" / "+std::to_wstring(total));}return 0;}
    case WM_APP_FINISHED:{g_running=false;ShowWindow(GetDlgItem(hwnd,IDC_CANCEL),SW_HIDE);ShowWindow(GetDlgItem(hwnd,IDC_START),SW_SHOW);SetText(g_status,wp?L"圧縮エンジンを初期化できませんでした":g_cancel?L"圧縮をキャンセルしました":L"圧縮が完了しました");return 0;}
    case WM_CLOSE:if(g_running){if(MessageBoxW(hwnd,L"圧縮を中止して終了しますか？",L"mknkCompress",MB_YESNO|MB_ICONQUESTION)!=IDYES)return 0;g_cancel=true;}DestroyWindow(hwnd);return 0;
    case WM_DESTROY:SaveSettings();g_cancel=true;if(g_worker.joinable())g_worker.join();DeleteObject(g_font);DeleteObject(g_titleFont);PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
  g_instance=instance;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&common);
  WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP_ICON));wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=kClassName;wc.hIconSm=wc.hIcon;RegisterClassExW(&wc);
  HWND hwnd=CreateWindowExW(0,kClassName,L"mknkCompress - WindowsネイティブJPG圧縮",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1180,790,nullptr,nullptr,instance,nullptr);
  if(!hwnd){CoUninitialize();return 1;}ShowWindow(hwnd,show);UpdateWindow(hwnd);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}CoUninitialize();return static_cast<int>(msg.wParam);
}
