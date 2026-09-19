#include "windows_asset_setup.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace melee::windows {
namespace {

constexpr int extract_button_id = 1001;
constexpr int explorer_button_id = 1002;
constexpr int close_button_id = 1003;
constexpr UINT_PTR progress_timer_id = 1004;

struct AssetSetupWindow {
    std::filesystem::path root;
    std::filesystem::path progress_file;
    HANDLE extraction_process = nullptr;
    HWND status = nullptr;
    HWND progress = nullptr;
    HWND extract = nullptr;
    HWND explorer = nullptr;
    HWND close = nullptr;
    bool complete = false;
};

std::filesystem::path executable_directory()
{
    std::array<wchar_t, 32768> filename{};
    const DWORD length = GetModuleFileNameW(
        nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    if (length == 0 || length >= filename.size()) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(filename.data()).parent_path();
}

AssetSetupWindow* state(HWND window)
{
    return reinterpret_cast<AssetSetupWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
}

std::wstring quoted(const std::filesystem::path& path)
{
    return L"\"" + path.wstring() + L"\"";
}

std::optional<std::filesystem::path> find_executable(const wchar_t* name)
{
    std::array<wchar_t, 32768> path{};
    const DWORD length = SearchPathW(nullptr, name, nullptr,
                                     static_cast<DWORD>(path.size()),
                                     path.data(), nullptr);
    if (length == 0 || length >= path.size()) {
        return std::nullopt;
    }
    return std::filesystem::path(path.data());
}

void set_default_font(HWND control)
{
    SendMessageW(control, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
}

void set_extraction_controls(AssetSetupWindow& window, bool enabled)
{
    EnableWindow(window.extract, enabled);
    EnableWindow(window.explorer, enabled);
    EnableWindow(window.close, enabled);
}

void update_progress(AssetSetupWindow& window)
{
    std::ifstream stream(window.progress_file);
    unsigned long long copied = 0;
    unsigned long long total = 0;
    if (!(stream >> copied >> total) || total == 0) {
        return;
    }
    const int percent = static_cast<int>(
        std::min(100ULL, (copied * 100ULL) / total));
    SendMessageW(window.progress, PBM_SETPOS, percent, 0);
    const std::wstring text = L"Extracting game files: " +
                              std::to_wstring(percent) + L"%";
    SetWindowTextW(window.status, text.c_str());
}

void finish_extraction(HWND window, AssetSetupWindow& setup)
{
    DWORD exit_code = 1;
    GetExitCodeProcess(setup.extraction_process, &exit_code);
    CloseHandle(setup.extraction_process);
    setup.extraction_process = nullptr;
    KillTimer(window, progress_timer_id);
    if (exit_code == 0 && has_local_assets(setup.root)) {
        setup.complete = true;
        SendMessageW(setup.progress, PBM_SETPOS, 100, 0);
        SetWindowTextW(setup.status, L"Extraction complete. Starting game...");
        DestroyWindow(window);
        return;
    }

    SetWindowTextW(setup.status,
                   L"Extraction failed. Check melee-extract.log, then try again.");
    set_extraction_controls(setup, true);
    MessageBoxW(window,
                L"The ISO could not be extracted. Details were written to "
                L"melee-extract.log in the project folder.",
                L"Extraction failed", MB_OK | MB_ICONERROR);
}

void start_extraction(HWND window, AssetSetupWindow& setup)
{
    std::array<wchar_t, 32768> image{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter =
        L"GameCube disc images (*.iso;*.gcm)\0*.iso;*.gcm\0"
        L"All files\0*.*\0";
    dialog.lpstrFile = image.data();
    dialog.nMaxFile = static_cast<DWORD>(image.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    dialog.lpstrTitle = L"Select your legal Super Smash Bros. Melee ISO/GCM";
    if (!GetOpenFileNameW(&dialog)) {
        return;
    }

    const auto python = find_executable(L"python.exe");
    if (!python.has_value()) {
        MessageBoxW(window, L"Python could not be found. Install Python 3 and try again.",
                    L"Python not found", MB_OK | MB_ICONERROR);
        return;
    }

    const auto destination = setup.root / "assets-local";
    const bool repair = std::filesystem::exists(destination);
    if (repair &&
        MessageBoxW(window,
                    L"An incomplete assets-local folder already exists. "
                    L"Repairing it will replace its extracted game files. Continue?",
                    L"Repair incomplete extraction", MB_YESNO | MB_ICONWARNING) !=
            IDYES) {
        return;
    }

    const auto extractor = setup.root / "tools" / "melee_extract.py";
    setup.progress_file = setup.root / ".melee-extract-progress";
    const auto log_file = setup.root / "melee-extract.log";
    std::error_code error;
    std::filesystem::remove(setup.progress_file, error);
    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;
    HANDLE log = CreateFileW(log_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                             &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                             nullptr);
    if (log == INVALID_HANDLE_VALUE) {
        MessageBoxW(window, L"Could not create melee-extract.log.",
                    L"Extraction failed to start", MB_OK | MB_ICONERROR);
        return;
    }

    HANDLE input = CreateFileW(L"NUL", GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &inheritable, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (input == INVALID_HANDLE_VALUE) {
        CloseHandle(log);
        MessageBoxW(window, L"Could not prepare the extraction process.",
                    L"Extraction failed to start", MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring command = quoted(*python) + L" " + quoted(extractor) +
                           L" extract " +
                           quoted(std::filesystem::path(image.data())) + L" " +
                           quoted(destination) + L" --progress-file " +
                           quoted(setup.progress_file);
    if (repair) {
        command += L" --force";
    }
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = log;
    startup.hStdError = log;
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(
        python->c_str(), command_buffer.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, setup.root.c_str(), &startup, &process);
    const DWORD start_error = started ? ERROR_SUCCESS : GetLastError();
    CloseHandle(input);
    CloseHandle(log);
    if (!started) {
        const std::wstring message =
            L"The extraction process could not be started (Windows error " +
            std::to_wstring(start_error) + L").";
        MessageBoxW(window, message.c_str(),
                    L"Extraction failed to start", MB_OK | MB_ICONERROR);
        return;
    }
    CloseHandle(process.hThread);
    setup.extraction_process = process.hProcess;
    SendMessageW(setup.progress, PBM_SETPOS, 0, 0);
    SetWindowTextW(setup.status, L"Preparing extraction...");
    set_extraction_controls(setup, false);
    SetTimer(window, progress_timer_id, 100, nullptr);
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam,
                                  LPARAM lparam)
{
    static_cast<void>(lparam);
    AssetSetupWindow* const setup = state(window);
    switch (message) {
    case WM_COMMAND:
        if (setup == nullptr) {
            return 0;
        }
        if (LOWORD(wparam) == extract_button_id) {
            start_extraction(window, *setup);
        } else if (LOWORD(wparam) == explorer_button_id) {
            ShellExecuteW(window, L"open", setup->root.c_str(), nullptr,
                          nullptr, SW_SHOWNORMAL);
        } else if (LOWORD(wparam) == close_button_id) {
            DestroyWindow(window);
        }
        return 0;
    case WM_TIMER:
        if (setup != nullptr && wparam == progress_timer_id) {
            update_progress(*setup);
            if (WaitForSingleObject(setup->extraction_process, 0) == WAIT_OBJECT_0) {
                finish_extraction(window, *setup);
            }
        }
        return 0;
    case WM_CLOSE:
        if (setup != nullptr && setup->extraction_process != nullptr) {
            MessageBoxW(window, L"Extraction is still in progress.",
                        L"Please wait", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (setup != nullptr) {
            if (setup->extraction_process != nullptr) {
                CloseHandle(setup->extraction_process);
            }
            const int result = setup->complete ? 1 : 0;
            delete setup;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            PostQuitMessage(result);
        }
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

} // namespace

std::filesystem::path find_project_root()
{
    const auto current_root = std::filesystem::current_path();
    const auto executable_root = executable_directory();
    for (const auto& start : { current_root, executable_root }) {
        for (auto candidate = start;; candidate = candidate.parent_path()) {
            if (std::filesystem::exists(candidate / "tools" / "melee_extract.py")) {
                return candidate;
            }
            const auto parent = candidate.parent_path();
            if (parent == candidate) {
                break;
            }
        }
    }
    return executable_root;
}

bool has_local_assets(const std::filesystem::path& project_root)
{
    const auto assets = project_root / "assets-local";
    return std::filesystem::is_regular_file(assets / "sys" / "main.dol") &&
           std::filesystem::is_regular_file(assets / "manifest.json") &&
           std::filesystem::is_regular_file(assets / "dvd-index.bin");
}

bool show_missing_assets_window(const std::filesystem::path& project_root)
{
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t class_name[] = L"MeleeAssetSetup";
    WNDCLASSW window_class{};
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    window_class.lpfnWndProc = window_procedure;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&window_class);

    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&controls);
    HWND window = CreateWindowExW(
        WS_EX_DLGMODALFRAME, class_name, L"Super Smash Bros. Melee PC Port",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        630, 330, nullptr, nullptr, instance, nullptr);
    if (window == nullptr) {
        return false;
    }
    auto* setup = new AssetSetupWindow{ .root = project_root };
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(setup));

    const HWND explanation = CreateWindowW(
        L"STATIC",
        L"Game files were not found.\n\n"
        L"This port does not include Nintendo content. Select an ISO/GCM "
        L"from your own legal copy of Super Smash Bros. Melee "
        L"(GALE01 NTSC-U 1.02).",
        WS_CHILD | WS_VISIBLE, 24, 24, 580, 92, window, nullptr, instance,
        nullptr);
    setup->status = CreateWindowW(L"STATIC", L"Ready to extract game files.",
                                   WS_CHILD | WS_VISIBLE, 24, 130, 580, 25,
                                   window, nullptr, instance, nullptr);
    setup->progress = CreateWindowW(PROGRESS_CLASSW, nullptr,
                                    WS_CHILD | WS_VISIBLE, 24, 160, 580, 24,
                                    window, nullptr, instance, nullptr);
    SendMessageW(setup->progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    setup->extract = CreateWindowW(
        L"BUTTON", L"Select ISO/GCM and extract",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 24, 215, 230, 34, window,
        reinterpret_cast<HMENU>(extract_button_id), instance, nullptr);
    setup->explorer = CreateWindowW(
        L"BUTTON", L"Open project folder", WS_CHILD | WS_VISIBLE, 267, 215,
        180, 34, window, reinterpret_cast<HMENU>(explorer_button_id), instance,
        nullptr);
    setup->close = CreateWindowW(
        L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE, 460, 215, 144, 34,
        window, reinterpret_cast<HMENU>(close_button_id), instance, nullptr);
    set_default_font(explanation);
    set_default_font(setup->status);
    set_default_font(setup->extract);
    set_default_font(setup->explorer);
    set_default_font(setup->close);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return message.wParam == 1;
}

} // namespace melee::windows
