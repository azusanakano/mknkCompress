#include <algorithm>
#include <atomic>
#include <charconv>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// GCC's static C++ runtime can otherwise select glibc 2.36's arc4random symbol.
// A local non-cryptographic generator is sufficient for the runtime's internal
// hash seeding and keeps the executable compatible with Linux Mint 21.x.
extern "C" unsigned int arc4random(void) noexcept {
  static std::atomic<unsigned long long> state{
    static_cast<unsigned long long>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^
    (static_cast<unsigned long long>(::getpid()) << 32)
  };
  unsigned long long current = state.load(std::memory_order_relaxed);
  for (;;) {
    unsigned long long next = current;
    next ^= next << 13;
    next ^= next >> 7;
    next ^= next << 17;
    if (state.compare_exchange_weak(current, next, std::memory_order_relaxed)) return static_cast<unsigned int>(next >> 16);
  }
}

extern "C" unsigned long __isoc23_strtoul(const char* text, char** end, int base) noexcept {
  const char* p = text;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') ++p;
  bool negative = false;
  if (*p == '+' || *p == '-') { negative = *p == '-'; ++p; }
  if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
  else if (base == 0) base = *p == '0' ? 8 : 10;
  const char* digits = p;
  unsigned long value = 0;
  while (*p) {
    unsigned int digit;
    if (*p >= '0' && *p <= '9') digit = static_cast<unsigned int>(*p - '0');
    else if (*p >= 'a' && *p <= 'z') digit = static_cast<unsigned int>(*p - 'a' + 10);
    else if (*p >= 'A' && *p <= 'Z') digit = static_cast<unsigned int>(*p - 'A' + 10);
    else break;
    if (digit >= static_cast<unsigned int>(base)) break;
    value = value * static_cast<unsigned long>(base) + digit;
    ++p;
  }
  if (end) *end = const_cast<char*>(p == digits ? text : p);
  return negative ? 0UL - value : value;
}

// GTK 3 is loaded from the Linux Mint runtime.  The small ABI declaration below
// deliberately avoids development-header dependencies in the portable build.
extern "C" {
using gboolean = int;
using gint = int;
using guint = unsigned int;
using gulong = unsigned long;
using gdouble = double;
using gchar = char;
using gpointer = void*;
using GCallback = void (*)();
using GClosureNotify = void (*)(gpointer, void*);
using GSourceFunc = gboolean (*)(gpointer);

struct _GtkWidget; using GtkWidget = _GtkWidget;
struct _GtkCssProvider; using GtkCssProvider = _GtkCssProvider;
struct _GdkScreen; using GdkScreen = _GdkScreen;
struct _GdkDragContext; using GdkDragContext = _GdkDragContext;
struct _GtkSelectionData; using GtkSelectionData = _GtkSelectionData;
struct _GtkFileFilter; using GtkFileFilter = _GtkFileFilter;
struct _GError { guint domain; gint code; gchar* message; }; using GError = _GError;
struct _GSList { gpointer data; _GSList* next; }; using GSList = _GSList;

void gtk_init(int*, char***);
gboolean gtk_init_check(int*, char***);
void gtk_main();
void gtk_main_quit();
GtkWidget* gtk_window_new(int);
void gtk_window_set_title(void*, const gchar*);
void gtk_window_set_default_size(void*, gint, gint);
void gtk_window_set_position(void*, int);
void gtk_container_set_border_width(void*, guint);
void gtk_container_add(void*, GtkWidget*);
GtkWidget* gtk_box_new(int, gint);
void gtk_box_pack_start(void*, GtkWidget*, gboolean, gboolean, guint);
void gtk_box_pack_end(void*, GtkWidget*, gboolean, gboolean, guint);
GtkWidget* gtk_grid_new();
void gtk_grid_set_row_spacing(void*, guint);
void gtk_grid_set_column_spacing(void*, guint);
void gtk_grid_attach(void*, GtkWidget*, gint, gint, gint, gint);
GtkWidget* gtk_label_new(const gchar*);
void gtk_label_set_text(void*, const gchar*);
void gtk_label_set_markup(void*, const gchar*);
void gtk_label_set_xalign(void*, float);
GtkWidget* gtk_button_new_with_label(const gchar*);
GtkWidget* gtk_check_button_new_with_label(const gchar*);
void gtk_toggle_button_set_active(void*, gboolean);
gboolean gtk_toggle_button_get_active(void*);
GtkWidget* gtk_entry_new();
void gtk_entry_set_text(void*, const gchar*);
const gchar* gtk_entry_get_text(void*);
void gtk_entry_set_width_chars(void*, gint);
GtkWidget* gtk_scale_new_with_range(int, gdouble, gdouble, gdouble);
void gtk_scale_set_draw_value(void*, gboolean);
void gtk_range_set_value(void*, gdouble);
gdouble gtk_range_get_value(void*);
GtkWidget* gtk_combo_box_text_new();
void gtk_combo_box_text_append_text(void*, const gchar*);
void gtk_combo_box_set_active(void*, gint);
gint gtk_combo_box_get_active(void*);
GtkWidget* gtk_scrolled_window_new(void*, void*);
void gtk_scrolled_window_set_policy(void*, int, int);
GtkWidget* gtk_list_box_new();
void gtk_list_box_insert(void*, GtkWidget*, gint);
GtkWidget* gtk_progress_bar_new();
void gtk_progress_bar_set_fraction(void*, gdouble);
void gtk_progress_bar_set_text(void*, const gchar*);
void gtk_progress_bar_set_show_text(void*, gboolean);
GtkWidget* gtk_separator_new(int);
void gtk_widget_set_size_request(GtkWidget*, gint, gint);
void gtk_widget_set_sensitive(GtkWidget*, gboolean);
void gtk_widget_set_name(GtkWidget*, const gchar*);
void gtk_widget_show_all(GtkWidget*);
void gtk_widget_destroy(GtkWidget*);
GtkWidget* gtk_file_chooser_dialog_new(const gchar*, void*, int, const gchar*, ...);
void gtk_file_chooser_set_select_multiple(void*, gboolean);
GSList* gtk_file_chooser_get_filenames(void*);
gchar* gtk_file_chooser_get_filename(void*);
void gtk_file_chooser_set_filename(void*, const gchar*);
void gtk_file_chooser_add_filter(void*, GtkFileFilter*);
GtkFileFilter* gtk_file_filter_new();
void gtk_file_filter_set_name(GtkFileFilter*, const gchar*);
void gtk_file_filter_add_pattern(GtkFileFilter*, const gchar*);
gint gtk_dialog_run(void*);
void gtk_drag_dest_set(GtkWidget*, int, void*, gint, int);
void gtk_drag_dest_add_uri_targets(GtkWidget*);
gchar** gtk_selection_data_get_uris(const GtkSelectionData*);
void gtk_drag_finish(GdkDragContext*, gboolean, gboolean, guint);
GtkCssProvider* gtk_css_provider_new();
gboolean gtk_css_provider_load_from_data(GtkCssProvider*, const gchar*, long, GError**);
void gtk_style_context_add_provider_for_screen(GdkScreen*, void*, guint);
GdkScreen* gdk_screen_get_default();
gulong g_signal_connect_data(gpointer, const gchar*, GCallback, gpointer, GClosureNotify, int);
guint g_idle_add(GSourceFunc, gpointer);
gchar* g_filename_from_uri(const gchar*, gchar**, GError**);
void g_strfreev(gchar**);
void g_slist_free(GSList*);
void g_free(gpointer);
void g_error_free(GError*);
void g_object_unref(gpointer);
}

namespace fs = std::filesystem;

namespace {
constexpr const char* kVersion = "3.0.0 Linux";
constexpr int GTK_WINDOW_TOPLEVEL = 0;
constexpr int GTK_WIN_POS_CENTER = 1;
constexpr int GTK_ORIENTATION_HORIZONTAL = 0;
constexpr int GTK_ORIENTATION_VERTICAL = 1;
constexpr int GTK_POLICY_AUTOMATIC = 1;
constexpr int GTK_FILE_CHOOSER_ACTION_OPEN = 0;
constexpr int GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER = 2;
constexpr int GTK_RESPONSE_ACCEPT = -3;
constexpr int GTK_RESPONSE_CANCEL = -6;
constexpr int GTK_DEST_DEFAULT_ALL = 7;
constexpr int GDK_ACTION_COPY = 2;
constexpr guint GTK_STYLE_PROVIDER_PRIORITY_APPLICATION = 600;

template <class T> void Connect(GtkWidget* widget, const char* signal, T callback, void* data = nullptr) {
  g_signal_connect_data(widget, signal, reinterpret_cast<GCallback>(callback), data, nullptr, 0);
}

struct Settings {
  int quality = 82;
  bool resize = false;
  int maxWidth = 1920;
  int maxHeight = 1080;
  bool preserveMetadata = false;
  bool onlyIfSmaller = true;
  bool preserveTimestamps = true;
  std::string outputDir;
  std::string suffix = "_compressed";
  int visualTheme = 0;
  bool light = false;
};

struct Item {
  fs::path input;
  std::uintmax_t inputSize = 0;
  std::uintmax_t outputSize = 0;
  std::string state = "待機中";
  std::string detail;
  bool pending = true;
  GtkWidget* row = nullptr;
  GtkWidget* nameLabel = nullptr;
  GtkWidget* sizeLabel = nullptr;
  GtkWidget* stateLabel = nullptr;
  GtkWidget* resultLabel = nullptr;
};

struct Widgets {
  GtkWidget* window = nullptr;
  GtkWidget* list = nullptr;
  GtkWidget* status = nullptr;
  GtkWidget* progress = nullptr;
  GtkWidget* quality = nullptr;
  GtkWidget* qualityValue = nullptr;
  GtkWidget* resize = nullptr;
  GtkWidget* width = nullptr;
  GtkWidget* height = nullptr;
  GtkWidget* metadata = nullptr;
  GtkWidget* smaller = nullptr;
  GtkWidget* timestamps = nullptr;
  GtkWidget* output = nullptr;
  GtkWidget* suffix = nullptr;
  GtkWidget* theme = nullptr;
  GtkWidget* light = nullptr;
  GtkWidget* addFiles = nullptr;
  GtkWidget* addFolder = nullptr;
  GtkWidget* clear = nullptr;
  GtkWidget* start = nullptr;
  GtkWidget* cancel = nullptr;
};

Widgets g_ui;
Settings g_settings;
std::vector<std::unique_ptr<Item>> g_items;
std::mutex g_mutex;
std::jthread g_worker;
std::atomic_bool g_running{false};
std::atomic_bool g_cancel{false};
std::atomic_bool g_closing{false};
std::atomic<pid_t> g_child{-1};
fs::path g_appRoot;

std::string FormatBytes(std::uintmax_t bytes) {
  static const char* units[] = {"B", "KB", "MB", "GB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 3) { value /= 1024.0; ++unit; }
  std::ostringstream out;
  if (unit == 0) out << static_cast<unsigned long long>(bytes);
  else out << std::fixed << std::setprecision(1) << value;
  out << ' ' << units[unit];
  return out.str();
}

std::string Trim(std::string text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) text.pop_back();
  const auto first = text.find_first_not_of(" \t\r\n");
  return first == std::string::npos ? std::string{} : text.substr(first);
}

int ParseInt(const std::string& text, int fallback, int low, int high) {
  int value = fallback;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} ? std::clamp(value, low, high) : fallback;
}

template <class T> bool ParseNumber(const std::string& text, T& value) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

fs::path ConfigPath() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  fs::path base;
  if (xdg && *xdg) base = xdg;
  else if (const char* home = std::getenv("HOME")) base = fs::path(home) / ".config";
  else base = fs::temp_directory_path();
  return base / "mknkCompress" / "settings.ini";
}

void LoadSettings() {
  std::ifstream in(ConfigPath());
  std::unordered_map<std::string, std::string> kv;
  std::string line;
  while (std::getline(in, line)) {
    const auto p = line.find('=');
    if (p != std::string::npos) kv[Trim(line.substr(0, p))] = Trim(line.substr(p + 1));
  }
  auto get = [&](const char* key, const char* fallback) { auto it = kv.find(key); return it == kv.end() ? std::string(fallback) : it->second; };
  g_settings.quality = ParseInt(get("quality", "82"), 82, 35, 96);
  g_settings.resize = get("resize", "0") == "1";
  g_settings.maxWidth = ParseInt(get("maxWidth", "1920"), 1920, 1, 50000);
  g_settings.maxHeight = ParseInt(get("maxHeight", "1080"), 1080, 1, 50000);
  g_settings.preserveMetadata = get("metadata", "0") == "1";
  g_settings.onlyIfSmaller = get("onlyIfSmaller", "1") != "0";
  g_settings.preserveTimestamps = get("timestamps", "1") != "0";
  g_settings.outputDir = get("outputDir", "");
  g_settings.suffix = get("suffix", "_compressed");
  g_settings.visualTheme = ParseInt(get("visualTheme", "0"), 0, 0, 1);
  g_settings.light = get("light", "0") == "1";
}

void SaveSettings() {
  const fs::path path = ConfigPath();
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  out << "quality=" << g_settings.quality << '\n'
      << "resize=" << g_settings.resize << '\n'
      << "maxWidth=" << g_settings.maxWidth << '\n'
      << "maxHeight=" << g_settings.maxHeight << '\n'
      << "metadata=" << g_settings.preserveMetadata << '\n'
      << "onlyIfSmaller=" << g_settings.onlyIfSmaller << '\n'
      << "timestamps=" << g_settings.preserveTimestamps << '\n'
      << "outputDir=" << g_settings.outputDir << '\n'
      << "suffix=" << g_settings.suffix << '\n'
      << "visualTheme=" << g_settings.visualTheme << '\n'
      << "light=" << g_settings.light << '\n';
}

void ReadSettingsFromUi() {
  g_settings.quality = static_cast<int>(gtk_range_get_value(g_ui.quality));
  g_settings.resize = gtk_toggle_button_get_active(g_ui.resize);
  g_settings.maxWidth = ParseInt(gtk_entry_get_text(g_ui.width), 1920, 1, 50000);
  g_settings.maxHeight = ParseInt(gtk_entry_get_text(g_ui.height), 1080, 1, 50000);
  g_settings.preserveMetadata = gtk_toggle_button_get_active(g_ui.metadata);
  g_settings.onlyIfSmaller = gtk_toggle_button_get_active(g_ui.smaller);
  g_settings.preserveTimestamps = gtk_toggle_button_get_active(g_ui.timestamps);
  g_settings.outputDir = gtk_entry_get_text(g_ui.output);
  g_settings.suffix = gtk_entry_get_text(g_ui.suffix);
  if (g_settings.suffix.empty()) g_settings.suffix = "_compressed";
  g_settings.visualTheme = std::max(0, gtk_combo_box_get_active(g_ui.theme));
  g_settings.light = gtk_toggle_button_get_active(g_ui.light);
}

void ApplyCss() {
  ReadSettingsFromUi();
  const bool gravity = g_settings.visualTheme == 1;
  const bool light = g_settings.light;
  const std::string bg = light ? "#eef6fa" : (gravity ? "#0f081e" : "#071523");
  const std::string panel = light ? "#ffffff" : (gravity ? "#1c1032" : "#0d2638");
  const std::string text = light ? "#182735" : "#ebf6fa";
  const std::string muted = light ? "#597082" : "#9bb3c3";
  const std::string accent = gravity ? "#915bff" : "#27d2e1";
  std::ostringstream css;
  css << "window{background:" << bg << ";color:" << text << ";}"
      << "label{color:" << text << ";}"
      << "#subtitle{color:" << muted << ";}"
      << "#panel{background:" << panel << ";border-radius:12px;padding:8px;}"
      << "button{background:" << panel << ";color:" << text << ";border:1px solid " << accent << ";border-radius:7px;padding:7px 12px;}"
      << "button:hover{background:" << accent << ";color:#07131b;}"
      << "#primary{background:" << accent << ";color:#07131b;font-weight:bold;}"
      << "entry,combobox{background:" << panel << ";color:" << text << ";border-color:" << accent << ";}"
      << "progressbar progress{background:" << accent << ";}"
      << "progressbar trough{background:" << panel << ";}"
      << "list{background:" << panel << ";color:" << text << ";}"
      << "scale highlight{background:" << accent << ";}";
  GtkCssProvider* provider = gtk_css_provider_new();
  GError* error = nullptr;
  gtk_css_provider_load_from_data(provider, css.str().c_str(), -1, &error);
  if (error) g_error_free(error);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

bool Supported(const fs::path& path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  static const std::unordered_set<std::string> extensions{
    ".jpg", ".jpeg", ".png", ".webp", ".gif", ".tif", ".tiff", ".svg", ".avif", ".heic", ".heif"
  };
  return extensions.contains(ext);
}

void AddRow(Item& item) {
  item.row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  item.nameLabel = gtk_label_new(item.input.filename().string().c_str());
  item.sizeLabel = gtk_label_new(FormatBytes(item.inputSize).c_str());
  item.stateLabel = gtk_label_new(item.state.c_str());
  item.resultLabel = gtk_label_new("—");
  gtk_label_set_xalign(item.nameLabel, 0.0f);
  gtk_label_set_xalign(item.stateLabel, 0.0f);
  gtk_label_set_xalign(item.resultLabel, 0.0f);
  gtk_widget_set_size_request(item.nameLabel, 340, -1);
  gtk_widget_set_size_request(item.sizeLabel, 90, -1);
  gtk_widget_set_size_request(item.stateLabel, 170, -1);
  gtk_widget_set_size_request(item.resultLabel, 160, -1);
  gtk_box_pack_start(item.row, item.nameLabel, true, true, 4);
  gtk_box_pack_start(item.row, item.sizeLabel, false, false, 4);
  gtk_box_pack_start(item.row, item.stateLabel, false, false, 4);
  gtk_box_pack_start(item.row, item.resultLabel, false, false, 4);
  gtk_list_box_insert(g_ui.list, item.row, -1);
  gtk_widget_show_all(item.row);
}

bool AddOne(const fs::path& raw) {
  std::error_code ec;
  const fs::path path = fs::weakly_canonical(raw, ec);
  if (ec || !fs::is_regular_file(path, ec) || !Supported(path)) return false;
  for (const auto& existing : g_items) if (existing->input == path) return false;
  auto item = std::make_unique<Item>();
  item->input = path;
  item->inputSize = fs::file_size(path, ec);
  AddRow(*item);
  g_items.push_back(std::move(item));
  return true;
}

bool AddPath(const fs::path& path) {
  std::error_code ec;
  bool added = false;
  if (fs::is_directory(path, ec)) {
    for (fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
      if (!ec && it->is_regular_file(ec)) added = AddOne(it->path()) || added;
    }
  } else added = AddOne(path);
  return added;
}

struct UiMessage {
  enum class Kind { Item, Progress, Finish } kind;
  size_t index = 0;
  double fraction = 0.0;
  std::string status;
};

gboolean ApplyUiMessage(gpointer data) {
  std::unique_ptr<UiMessage> message(static_cast<UiMessage*>(data));
  if (g_closing) return 0;
  if (message->kind == UiMessage::Kind::Item && message->index < g_items.size()) {
    std::lock_guard lock(g_mutex);
    auto& item = *g_items[message->index];
    gtk_label_set_text(item.stateLabel, item.state.c_str());
    gtk_label_set_text(item.resultLabel, item.detail.empty() ? "—" : item.detail.c_str());
  } else if (message->kind == UiMessage::Kind::Progress) {
    gtk_progress_bar_set_fraction(g_ui.progress, message->fraction);
    gtk_progress_bar_set_text(g_ui.progress, message->status.c_str());
    gtk_label_set_text(g_ui.status, message->status.c_str());
  } else {
    g_running = false;
    gtk_widget_set_sensitive(g_ui.addFiles, true);
    gtk_widget_set_sensitive(g_ui.addFolder, true);
    gtk_widget_set_sensitive(g_ui.clear, true);
    gtk_widget_set_sensitive(g_ui.start, true);
    gtk_widget_set_sensitive(g_ui.cancel, false);
    gtk_progress_bar_set_fraction(g_ui.progress, 1.0);
    gtk_progress_bar_set_text(g_ui.progress, message->status.c_str());
    gtk_label_set_text(g_ui.status, message->status.c_str());
  }
  return 0;
}

void Post(UiMessage message) { g_idle_add(ApplyUiMessage, new UiMessage(std::move(message))); }

struct ProcessResult {
  std::string tag;
  std::uintmax_t size = 0;
  int quality = 0;
  int candidates = 0;
  double score = 0.0;
  std::string message;
};

std::vector<std::string> SplitTabs(const std::string& line) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    const size_t p = line.find('\t', start);
    parts.push_back(line.substr(start, p == std::string::npos ? p : p - start));
    if (p == std::string::npos) break;
    start = p + 1;
  }
  return parts;
}

ProcessResult RunBackend(const fs::path& input, const fs::path& output, const Settings& settings) {
  const fs::path node = g_appRoot / "runtime" / "node";
  const fs::path script = g_appRoot / "app" / "compress.mjs";
  std::vector<std::string> args{
    node.string(), script.string(), "--input", input.string(), "--output", output.string(),
    "--quality", std::to_string(settings.quality), "--resize", settings.resize ? "1" : "0",
    "--width", std::to_string(settings.maxWidth), "--height", std::to_string(settings.maxHeight),
    "--metadata", settings.preserveMetadata ? "1" : "0", "--smaller", settings.onlyIfSmaller ? "1" : "0",
    "--timestamps", settings.preserveTimestamps ? "1" : "0"
  };
  int pipefd[2];
  if (pipe(pipefd) != 0) return {"ERROR", 0, 0, 0, 0.0, std::strerror(errno)};
  const pid_t pid = fork();
  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]); close(pipefd[1]);
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& arg : args) argv.push_back(arg.data());
    argv.push_back(nullptr);
    execv(argv[0], argv.data());
    _exit(127);
  }
  close(pipefd[1]);
  if (pid < 0) { close(pipefd[0]); return {"ERROR", 0, 0, 0, 0.0, std::strerror(errno)}; }
  g_child = pid;
  std::string outputText;
  char buffer[4096];
  ssize_t n;
  while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0) outputText.append(buffer, static_cast<size_t>(n));
  close(pipefd[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  g_child = -1;
  std::istringstream lines(outputText);
  std::string line, protocol;
  while (std::getline(lines, line)) if (line.rfind("OK\t", 0) == 0 || line.rfind("SKIP\t", 0) == 0 || line.rfind("ERROR\t", 0) == 0) protocol = line;
  if (g_cancel) return {"CANCEL", 0, 0, 0, 0.0, "キャンセル"};
  const auto parts = SplitTabs(protocol);
  if (parts.size() >= 6 && parts[0] == "OK") {
    std::uintmax_t size = 0; int quality = 0; int candidates = 0; double score = 0.0;
    if (ParseNumber(parts[1], size) && ParseNumber(parts[2], quality) && ParseNumber(parts[3], candidates) && ParseNumber(parts[4], score))
      return {parts[0], size, quality, candidates, score, parts[5]};
  }
  if (parts.size() >= 2 && parts[0] == "SKIP") return {parts[0], 0, 0, 0, 0.0, parts[1]};
  if (parts.size() >= 2 && parts[0] == "ERROR") return {parts[0], 0, 0, 0, 0.0, parts[1]};
  return {"ERROR", 0, 0, 0, 0.0, protocol.empty() ? Trim(outputText) : protocol};
}

fs::path OutputPathFor(const fs::path& input, const Settings& settings) {
  fs::path dir = settings.outputDir.empty() ? input.parent_path() : fs::path(settings.outputDir);
  std::error_code ec;
  fs::create_directories(dir, ec);
  fs::path output = dir / (input.stem().string() + settings.suffix + ".jpg");
  if (output == input || fs::exists(output, ec)) {
    const std::string base = input.stem().string() + settings.suffix;
    int n = 2;
    do { output = dir / (base + "-" + std::to_string(n++) + ".jpg"); } while (fs::exists(output, ec));
  }
  return output;
}

void Worker(Settings settings) {
  std::vector<size_t> pending;
  {
    std::lock_guard lock(g_mutex);
    for (size_t i = 0; i < g_items.size(); ++i) if (g_items[i]->pending) pending.push_back(i);
  }
  size_t completed = 0;
  for (const size_t index : pending) {
    if (g_cancel) break;
    {
      std::lock_guard lock(g_mutex);
      g_items[index]->state = "圧縮中…";
      g_items[index]->detail.clear();
    }
    Post({UiMessage::Kind::Item, index, 0.0, {}});
    Post({UiMessage::Kind::Progress, 0, pending.empty() ? 0.0 : static_cast<double>(completed) / pending.size(),
          std::to_string(completed + 1) + " / " + std::to_string(pending.size()) + " を処理中"});
    const fs::path output = OutputPathFor(g_items[index]->input, settings);
    const ProcessResult result = RunBackend(g_items[index]->input, output, settings);
    {
      std::lock_guard lock(g_mutex);
      auto& item = *g_items[index];
      item.pending = false;
      if (result.tag == "OK") {
        item.outputSize = result.size;
        const double reduction = item.inputSize ? 100.0 * static_cast<double>(item.inputSize - std::min(item.inputSize, item.outputSize)) / static_cast<double>(item.inputSize) : 0.0;
        std::ostringstream detail;
        detail << FormatBytes(item.outputSize) << "  (-" << std::fixed << std::setprecision(1) << reduction << "%)";
        item.detail = detail.str();
        item.state = "完了  Q" + std::to_string(result.quality);
      } else if (result.tag == "SKIP") {
        item.state = "保存なし";
        item.detail = result.message;
      } else if (result.tag == "CANCEL") {
        item.pending = true;
        item.state = "待機中";
        item.detail = "キャンセル";
      } else {
        item.state = "エラー";
        item.detail = result.message.empty() ? "処理に失敗" : result.message;
      }
    }
    Post({UiMessage::Kind::Item, index, 0.0, {}});
    ++completed;
  }
  const std::string status = g_cancel ? "圧縮をキャンセルしました" : std::to_string(completed) + "件の処理が完了しました";
  Post({UiMessage::Kind::Finish, 0, 1.0, status});
}

void StartProcessing() {
  if (g_running) return;
  bool any = false;
  for (const auto& item : g_items) any = any || item->pending;
  if (!any) { gtk_label_set_text(g_ui.status, "待機中の画像がありません"); return; }
  ReadSettingsFromUi();
  gtk_entry_set_text(g_ui.suffix, g_settings.suffix.c_str());
  SaveSettings();
  g_cancel = false;
  g_running = true;
  gtk_widget_set_sensitive(g_ui.addFiles, false);
  gtk_widget_set_sensitive(g_ui.addFolder, false);
  gtk_widget_set_sensitive(g_ui.clear, false);
  gtk_widget_set_sensitive(g_ui.start, false);
  gtk_widget_set_sensitive(g_ui.cancel, true);
  if (g_worker.joinable()) g_worker.join();
  g_worker = std::jthread(Worker, g_settings);
}

void OnAddFiles(GtkWidget*, gpointer) {
  GtkWidget* dialog = gtk_file_chooser_dialog_new("画像を追加", g_ui.window, GTK_FILE_CHOOSER_ACTION_OPEN,
      "キャンセル", GTK_RESPONSE_CANCEL, "追加", GTK_RESPONSE_ACCEPT, nullptr);
  gtk_file_chooser_set_select_multiple(dialog, true);
  GtkFileFilter* filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, "対応画像");
  for (const char* pattern : {"*.jpg", "*.jpeg", "*.png", "*.webp", "*.gif", "*.tif", "*.tiff", "*.svg", "*.avif", "*.heic", "*.heif"})
    gtk_file_filter_add_pattern(filter, pattern);
  gtk_file_chooser_add_filter(dialog, filter);
  if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
    GSList* files = gtk_file_chooser_get_filenames(dialog);
    for (GSList* node = files; node; node = node->next) { AddPath(static_cast<char*>(node->data)); g_free(node->data); }
    g_slist_free(files);
    gtk_label_set_text(g_ui.status, "追加しました。『圧縮開始』を押してください");
  }
  gtk_widget_destroy(dialog);
}

void OnAddFolder(GtkWidget*, gpointer) {
  GtkWidget* dialog = gtk_file_chooser_dialog_new("フォルダーを追加", g_ui.window, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      "キャンセル", GTK_RESPONSE_CANCEL, "追加", GTK_RESPONSE_ACCEPT, nullptr);
  if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
    gchar* folder = gtk_file_chooser_get_filename(dialog);
    if (folder) { AddPath(folder); g_free(folder); }
    gtk_label_set_text(g_ui.status, "フォルダーを追加しました。『圧縮開始』を押してください");
  }
  gtk_widget_destroy(dialog);
}

void OnBrowseOutput(GtkWidget*, gpointer) {
  GtkWidget* dialog = gtk_file_chooser_dialog_new("出力先を選択", g_ui.window, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      "キャンセル", GTK_RESPONSE_CANCEL, "選択", GTK_RESPONSE_ACCEPT, nullptr);
  const char* current = gtk_entry_get_text(g_ui.output);
  if (current && *current) gtk_file_chooser_set_filename(dialog, current);
  if (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT) {
    gchar* folder = gtk_file_chooser_get_filename(dialog);
    if (folder) { gtk_entry_set_text(g_ui.output, folder); g_free(folder); }
  }
  gtk_widget_destroy(dialog);
}

void OnClear(GtkWidget*, gpointer) {
  if (g_running) return;
  for (auto& item : g_items) if (item->row) gtk_widget_destroy(item->row);
  g_items.clear();
  gtk_label_set_text(g_ui.status, "画像をここへドロップしてください");
  gtk_progress_bar_set_fraction(g_ui.progress, 0.0);
  gtk_progress_bar_set_text(g_ui.progress, "待機中");
}

void OnStart(GtkWidget*, gpointer) { StartProcessing(); }

void OnCancel(GtkWidget*, gpointer) {
  g_cancel = true;
  const pid_t child = g_child.load();
  if (child > 0) kill(child, SIGTERM);
  gtk_label_set_text(g_ui.status, "キャンセル中…");
}

void SetPreset(int quality, bool resize, int width, int height) {
  gtk_range_set_value(g_ui.quality, quality);
  gtk_toggle_button_set_active(g_ui.resize, resize);
  gtk_entry_set_text(g_ui.width, std::to_string(width).c_str());
  gtk_entry_set_text(g_ui.height, std::to_string(height).c_str());
}

void OnPreset(GtkWidget*, gpointer data) {
  const auto preset = reinterpret_cast<std::intptr_t>(data);
  if (preset == 0) SetPreset(82, false, 1920, 1080);
  else if (preset == 1) SetPreset(90, false, 1920, 1080);
  else if (preset == 2) SetPreset(74, true, 1920, 1080);
  else SetPreset(64, true, 1280, 1280);
}

void OnQualityChanged(GtkWidget*, gpointer) {
  const std::string value = std::to_string(static_cast<int>(gtk_range_get_value(g_ui.quality)));
  gtk_label_set_text(g_ui.qualityValue, value.c_str());
}

void OnThemeChanged(GtkWidget*, gpointer) { ApplyCss(); }

void OnDrop(GtkWidget*, GdkDragContext* context, gint, gint, GtkSelectionData* data, guint, guint time, gpointer) {
  bool added = false;
  gchar** uris = gtk_selection_data_get_uris(data);
  if (uris) {
    for (size_t i = 0; uris[i]; ++i) {
      GError* error = nullptr;
      gchar* filename = g_filename_from_uri(uris[i], nullptr, &error);
      if (filename) { added = AddPath(filename) || added; g_free(filename); }
      if (error) g_error_free(error);
    }
    g_strfreev(uris);
  }
  gtk_drag_finish(context, added, false, time);
  if (added) StartProcessing();
}

void OnDestroy(GtkWidget*, gpointer) {
  g_closing = true;
  g_cancel = true;
  const pid_t child = g_child.load();
  if (child > 0) kill(child, SIGTERM);
  if (g_worker.joinable()) g_worker.join();
  gtk_main_quit();
}

GtkWidget* LabeledEntry(GtkWidget* grid, const char* label, int row, GtkWidget*& entry, const std::string& value, int widthChars = 12) {
  GtkWidget* title = gtk_label_new(label);
  gtk_label_set_xalign(title, 0.0f);
  entry = gtk_entry_new();
  gtk_entry_set_text(entry, value.c_str());
  gtk_entry_set_width_chars(entry, widthChars);
  gtk_grid_attach(grid, title, 0, row, 1, 1);
  gtk_grid_attach(grid, entry, 1, row, 1, 1);
  return entry;
}

void BuildUi() {
  g_ui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(g_ui.window, "mknkCompress 3.0.0 Linux");
  gtk_window_set_default_size(g_ui.window, 1040, 780);
  gtk_window_set_position(g_ui.window, GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(g_ui.window, 18);
  Connect(g_ui.window, "destroy", OnDestroy);
  gtk_drag_dest_set(g_ui.window, GTK_DEST_DEFAULT_ALL, nullptr, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets(g_ui.window);
  Connect(g_ui.window, "drag-data-received", OnDrop);

  GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add(g_ui.window, root);

  GtkWidget* title = gtk_label_new(nullptr);
  gtk_label_set_markup(title, "<span size='xx-large' weight='bold'>mknkCompress</span>  <span size='small'>3.0.0 Linux</span>");
  gtk_label_set_xalign(title, 0.0f);
  gtk_box_pack_start(root, title, false, false, 0);
  GtkWidget* subtitle = gtk_label_new("JPG専用・完全ローカル画像圧縮　画像／フォルダーをドロップすると自動開始します");
  gtk_widget_set_name(subtitle, "subtitle");
  gtk_label_set_xalign(subtitle, 0.0f);
  gtk_box_pack_start(root, subtitle, false, false, 0);

  GtkWidget* topButtons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  g_ui.addFiles = gtk_button_new_with_label("画像を追加");
  g_ui.addFolder = gtk_button_new_with_label("フォルダーを追加");
  g_ui.clear = gtk_button_new_with_label("一覧を消去");
  g_ui.start = gtk_button_new_with_label("圧縮開始");
  g_ui.cancel = gtk_button_new_with_label("キャンセル");
  gtk_widget_set_name(g_ui.start, "primary");
  gtk_widget_set_sensitive(g_ui.cancel, false);
  Connect(g_ui.addFiles, "clicked", OnAddFiles);
  Connect(g_ui.addFolder, "clicked", OnAddFolder);
  Connect(g_ui.clear, "clicked", OnClear);
  Connect(g_ui.start, "clicked", OnStart);
  Connect(g_ui.cancel, "clicked", OnCancel);
  for (GtkWidget* button : {g_ui.addFiles, g_ui.addFolder, g_ui.clear}) gtk_box_pack_start(topButtons, button, false, false, 0);
  gtk_box_pack_end(topButtons, g_ui.cancel, false, false, 0);
  gtk_box_pack_end(topButtons, g_ui.start, false, false, 0);
  gtk_box_pack_start(root, topButtons, false, false, 0);

  GtkWidget* body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start(root, body, true, true, 0);
  GtkWidget* left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_name(left, "panel");
  gtk_widget_set_name(right, "panel");
  gtk_box_pack_start(body, left, true, true, 0);
  gtk_box_pack_start(body, right, false, false, 0);
  gtk_widget_set_size_request(right, 310, -1);

  GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  for (auto [text, width] : std::vector<std::pair<const char*, int>>{{"ファイル",340},{"元サイズ",90},{"状態",170},{"結果",160}}) {
    GtkWidget* label = gtk_label_new(text); gtk_label_set_xalign(label, 0.0f); gtk_widget_set_size_request(label, width, -1); gtk_box_pack_start(header, label, true, true, 4);
  }
  gtk_box_pack_start(left, header, false, false, 0);
  GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_scrolled_window_set_policy(scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  g_ui.list = gtk_list_box_new();
  gtk_container_add(scroll, g_ui.list);
  gtk_box_pack_start(left, scroll, true, true, 0);

  GtkWidget* settingsTitle = gtk_label_new(nullptr);
  gtk_label_set_markup(settingsTitle, "<b>圧縮設定</b>");
  gtk_label_set_xalign(settingsTitle, 0.0f);
  gtk_box_pack_start(right, settingsTitle, false, false, 0);
  GtkWidget* qualityRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget* qualityTitle = gtk_label_new("JPG画質");
  g_ui.quality = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 35, 96, 1);
  gtk_scale_set_draw_value(g_ui.quality, false);
  gtk_range_set_value(g_ui.quality, g_settings.quality);
  g_ui.qualityValue = gtk_label_new(std::to_string(g_settings.quality).c_str());
  Connect(g_ui.quality, "value-changed", OnQualityChanged);
  gtk_box_pack_start(qualityRow, qualityTitle, false, false, 0);
  gtk_box_pack_start(qualityRow, g_ui.quality, true, true, 0);
  gtk_box_pack_start(qualityRow, g_ui.qualityValue, false, false, 0);
  gtk_box_pack_start(right, qualityRow, false, false, 0);

  GtkWidget* presets = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  for (auto [label, id] : std::vector<std::pair<const char*, std::intptr_t>>{{"標準",0},{"高画質",1},{"小容量",2},{"極小",3}}) {
    GtkWidget* button = gtk_button_new_with_label(label); Connect(button, "clicked", OnPreset, reinterpret_cast<void*>(id)); gtk_box_pack_start(presets, button, true, true, 0);
  }
  gtk_box_pack_start(right, presets, false, false, 0);

  g_ui.resize = gtk_check_button_new_with_label("指定サイズ内へ縮小");
  gtk_toggle_button_set_active(g_ui.resize, g_settings.resize);
  gtk_box_pack_start(right, g_ui.resize, false, false, 0);
  GtkWidget* dimensions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_ui.width = gtk_entry_new(); g_ui.height = gtk_entry_new();
  gtk_entry_set_text(g_ui.width, std::to_string(g_settings.maxWidth).c_str());
  gtk_entry_set_text(g_ui.height, std::to_string(g_settings.maxHeight).c_str());
  gtk_entry_set_width_chars(g_ui.width, 7); gtk_entry_set_width_chars(g_ui.height, 7);
  gtk_box_pack_start(dimensions, gtk_label_new("最大幅"), false, false, 0);
  gtk_box_pack_start(dimensions, g_ui.width, true, true, 0);
  gtk_box_pack_start(dimensions, gtk_label_new("高さ"), false, false, 0);
  gtk_box_pack_start(dimensions, g_ui.height, true, true, 0);
  gtk_box_pack_start(right, dimensions, false, false, 0);

  g_ui.metadata = gtk_check_button_new_with_label("EXIF・ICCメタデータを保持");
  g_ui.smaller = gtk_check_button_new_with_label("元画像より小さい場合だけ保存");
  g_ui.timestamps = gtk_check_button_new_with_label("ファイル日時を保持");
  gtk_toggle_button_set_active(g_ui.metadata, g_settings.preserveMetadata);
  gtk_toggle_button_set_active(g_ui.smaller, g_settings.onlyIfSmaller);
  gtk_toggle_button_set_active(g_ui.timestamps, g_settings.preserveTimestamps);
  gtk_box_pack_start(right, g_ui.metadata, false, false, 0);
  gtk_box_pack_start(right, g_ui.smaller, false, false, 0);
  gtk_box_pack_start(right, g_ui.timestamps, false, false, 0);
  gtk_box_pack_start(right, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), false, false, 4);

  GtkWidget* grid = gtk_grid_new();
  gtk_grid_set_row_spacing(grid, 7); gtk_grid_set_column_spacing(grid, 7);
  LabeledEntry(grid, "出力先", 0, g_ui.output, g_settings.outputDir, 18);
  GtkWidget* browse = gtk_button_new_with_label("選択"); Connect(browse, "clicked", OnBrowseOutput); gtk_grid_attach(grid, browse, 2, 0, 1, 1);
  LabeledEntry(grid, "接尾辞", 1, g_ui.suffix, g_settings.suffix, 14);
  gtk_box_pack_start(right, grid, false, false, 0);

  GtkWidget* appearance = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
  gtk_box_pack_start(appearance, gtk_label_new("テーマ"), false, false, 0);
  g_ui.theme = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(g_ui.theme, "反重力斥力場");
  gtk_combo_box_text_append_text(g_ui.theme, "重力圧縮");
  gtk_combo_box_set_active(g_ui.theme, g_settings.visualTheme);
  g_ui.light = gtk_check_button_new_with_label("ライト");
  gtk_toggle_button_set_active(g_ui.light, g_settings.light);
  Connect(g_ui.theme, "changed", OnThemeChanged);
  Connect(g_ui.light, "toggled", OnThemeChanged);
  gtk_box_pack_start(appearance, g_ui.theme, true, true, 0);
  gtk_box_pack_start(appearance, g_ui.light, false, false, 0);
  gtk_box_pack_start(right, appearance, false, false, 0);

  g_ui.progress = gtk_progress_bar_new();
  gtk_progress_bar_set_show_text(g_ui.progress, true);
  gtk_progress_bar_set_text(g_ui.progress, "待機中");
  g_ui.status = gtk_label_new("画像をここへドロップしてください");
  gtk_label_set_xalign(g_ui.status, 0.0f);
  gtk_box_pack_start(root, g_ui.progress, false, false, 0);
  gtk_box_pack_start(root, g_ui.status, false, false, 0);
  gtk_widget_show_all(g_ui.window);
  ApplyCss();
}

bool SelfTest() {
  const fs::path node = g_appRoot / "runtime" / "node";
  const fs::path script = g_appRoot / "app" / "compress.mjs";
  if (!fs::is_regular_file(node) || !fs::is_regular_file(script)) {
    std::cerr << "SELF-TEST FAIL: bundled runtime missing\n";
    return false;
  }
  const std::string command = "\"" + node.string() + "\" \"" + script.string() + "\" --self-test";
  const int status = std::system(command.c_str());
  if (status != 0) { std::cerr << "SELF-TEST FAIL: backend\n"; return false; }
  std::cout << "SELF-TEST PASS: GUI/runtime layout\n";
  return true;
}

fs::path ExecutableRoot() {
  std::vector<char> path(4096);
  const ssize_t n = readlink("/proc/self/exe", path.data(), path.size() - 1);
  if (n <= 0) return fs::current_path();
  path[static_cast<size_t>(n)] = '\0';
  return fs::path(path.data()).parent_path();
}
} // namespace

int main(int argc, char** argv) {
  g_appRoot = ExecutableRoot();
  if (argc > 1 && std::string(argv[1]) == "--self-test") return SelfTest() ? 0 : 1;
  if (!gtk_init_check(&argc, &argv)) {
    std::cerr << "mknkCompress: GUIを開始できません。Linux Mintのデスクトップセッションで実行してください。\n";
    return 2;
  }
  LoadSettings();
  BuildUi();
  for (int i = 1; i < argc; ++i) AddPath(argv[i]);
  if (argc > 1 && !g_items.empty()) StartProcessing();
  gtk_main();
  return 0;
}
