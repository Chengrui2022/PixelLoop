#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>
#include "arnold.h"

namespace {
using Gdiplus::Bitmap;
using Gdiplus::BitmapData;
using Gdiplus::Ok;

enum ID {
    Encrypt = 1001, Decrypt, Open, Sample, Save, Transfer, Run, ParamA, ParamB, Rounds
};

struct Image {
    int size = 0;
    std::vector<uint32_t> pixels;
    explicit operator bool() const { return size != 0; }
};

struct Page {
    Image input;
    Image output;
    Image reference;
    ArnoldKey key = {1, 1, 1};
    std::wstring name;
    std::wstring status;
    bool error = false;
};

HINSTANCE instance;
HWND window;
HWND brand, heading, hint, input_label, output_label, file_label;
HWND matrix_label, condition_label, status_label, parameters_label, a_label, b_label, rounds_label;
HWND encrypt_button, decrypt_button, open_button, sample_button, save_button, transfer_button, run_button;
HWND edit_a, edit_b, edit_rounds;
RECT input_rect, output_rect;
HFONT body_font, title_font;
HBRUSH background;
Page pages[2];
bool decrypt_page = false;
bool filling_fields = false;
int dpi = 96;

int px(int value) { return MulDiv(value, dpi, 96); }
Page &current() { return pages[decrypt_page ? 1 : 0]; }

std::wstring caption(const wchar_t *text, const Image &image) {
    return image ? std::wstring(text) + L"    " + std::to_wstring(image.size) + L" × " +
        std::to_wstring(image.size) : text;
}

void show_status(const wchar_t *text, bool error = false) {
    current().status = text;
    current().error = error;
    SetWindowTextW(status_label, text);
    InvalidateRect(status_label, nullptr, TRUE);
}

bool read_key(ArnoldKey &key) {
    BOOL valid_a = FALSE, valid_b = FALSE, valid_rounds = FALSE;
    const UINT a = GetDlgItemInt(window, ParamA, &valid_a, FALSE);
    const UINT b = GetDlgItemInt(window, ParamB, &valid_b, FALSE);
    const UINT rounds = GetDlgItemInt(window, Rounds, &valid_rounds, FALSE);
    if (!valid_a || !valid_b || !valid_rounds || a < 1 || a > 99 || b < 1 || b > 99 ||
        rounds < 1 || rounds > 9999) return false;
    key = {static_cast<int>(a), static_cast<int>(b), static_cast<int>(rounds)};
    return true;
}

void refresh_result() {
    const auto &page = current();
    SetWindowTextW(input_label, caption(decrypt_page ? L"置乱图" : L"原图", page.input).c_str());
    SetWindowTextW(output_label, caption(decrypt_page ? L"恢复图" : L"置乱图", page.output).c_str());
    EnableWindow(run_button, static_cast<bool>(page.input));
    EnableWindow(save_button, static_cast<bool>(page.output));
    EnableWindow(transfer_button, static_cast<bool>(page.output));
    InvalidateRect(window, &input_rect, FALSE);
    InvalidateRect(window, &output_rect, FALSE);
}

void refresh_matrix() {
    ArnoldKey key;
    if (!read_key(key)) {
        SetWindowTextW(matrix_label, L"请输入有效的整数参数。");
        return;
    }
    const int a = key.a, b = key.b;
    wchar_t text[96];
    if (decrypt_page) {
        swprintf(text, 96, L"逆矩阵 A⁻¹（模 N）\n[ %d    %d ]\n[ %d    1 ]", 1 + a*b, -a, -b);
    } else {
        swprintf(text, 96, L"变换矩阵 A（模 N）\n[ 1    %d ]\n[ %d    %d ]", a, b, 1 + a*b);
    }
    SetWindowTextW(matrix_label, text);
}

void refresh_page() {
    const auto &page = current();
    CheckRadioButton(window, Encrypt, Decrypt, decrypt_page ? Decrypt : Encrypt);
    SetWindowTextW(heading, decrypt_page ? L"图像解密" : L"图像加密");
    SetWindowTextW(hint, decrypt_page ? L"用逆矩阵恢复像素位置。请使用加密时的 a、b 和轮数 k。"
                                      : L"改变矩阵参数和轮数，观察图像置乱前后的变化。");
    SetWindowTextW(open_button, decrypt_page ? L"打开置乱 PNG" : L"打开图片");
    ShowWindow(sample_button, decrypt_page ? SW_HIDE : SW_SHOW);
    ShowWindow(transfer_button, decrypt_page ? SW_HIDE : SW_SHOW);
    SetWindowTextW(run_button, decrypt_page ? L"运行解密" : L"运行加密");
    SetWindowTextW(condition_label, decrypt_page ? L"保留图像尺寸和像素值。\n中间结果请使用无损 PNG。"
                                                 : L"a = b = 1 时为课件的变换。\n行列式为 1，所以模 N 可逆。");
    SetWindowTextW(file_label, page.name.empty() ? L"方形图像 · 2–1024 像素 · 无透明像素" : page.name.c_str());
    SetWindowTextW(status_label, page.status.c_str());
    InvalidateRect(status_label, nullptr, TRUE);
    filling_fields = true;
    SetDlgItemInt(window, ParamA, page.key.a, FALSE);
    SetDlgItemInt(window, ParamB, page.key.b, FALSE);
    SetDlgItemInt(window, Rounds, page.key.rounds, FALSE);
    filling_fields = false;
    refresh_matrix();
    refresh_result();
}

bool copy_bitmap(Bitmap &bitmap, Image &image) {
    const UINT width = bitmap.GetWidth(), height = bitmap.GetHeight();
    if (bitmap.GetLastStatus() != Ok || !width || !height) {
        show_status(L"无法读取该图像。", true);
        return false;
    }
    if (width != height || width < 2 || width > 1024) {
        show_status(L"请选择方形图片，边长须在 2 到 1024 像素之间。", true);
        return false;
    }
    const int size = static_cast<int>(width);
    Image loaded{size, std::vector<uint32_t>(size * size)};
    Gdiplus::Rect rect(0, 0, size, size);
    BitmapData data{};
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) {
        show_status(L"无法读取图像像素。", true);
        return false;
    }
    for (int row = 0; row < size; ++row) {
        const auto *line = static_cast<const BYTE *>(data.Scan0) + static_cast<ptrdiff_t>(row) * data.Stride;
        memcpy(loaded.pixels.data() + row * size, line, size * sizeof(uint32_t));
    }
    bitmap.UnlockBits(&data);
    if (std::any_of(loaded.pixels.begin(), loaded.pixels.end(), [](uint32_t pixel) { return (pixel >> 24) != 255; })) {
        show_status(L"请选择没有透明像素的图片。", true);
        return false;
    }
    image = std::move(loaded);
    return true;
}

void use_input(Image image, const wchar_t *name) {
    auto &page = current();
    page.input = std::move(image);
    page.output = {};
    page.reference = {};
    page.name = name;
    show_status(L"已载入图像，等待运行。");
    refresh_page();
}

void load_sample() {
    static const BYTE source[] = {
#include "sample.inc"
    };
    constexpr size_t bytes = sizeof(source);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        show_status(L"无法读取内置示例。", true);
        return;
    }
    void *destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        show_status(L"无法载入内置示例。", true);
        return;
    }
    memcpy(destination, source, bytes);
    GlobalUnlock(memory);
    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        show_status(L"无法载入内置示例。", true);
        return;
    }
    Image image;
    {
        Bitmap bitmap(stream, FALSE);
        copy_bitmap(bitmap, image);
    }
    stream->Release();
    if (image) use_input(std::move(image), L"示例图 · arnold_sample.png");
}

void open_image() {
    wchar_t filename[32768] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = decrypt_page ? L"置乱 PNG\0*.png\0\0"
                                      : L"PNG 或 JPEG 图像\0*.png;*.jpg;*.jpeg\0\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = ARRAYSIZE(filename);
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) return;

    Bitmap bitmap(filename, FALSE);
    GUID format{};
    const bool supported = bitmap.GetRawFormat(&format) == Ok &&
        (IsEqualGUID(format, Gdiplus::ImageFormatPNG) ||
         (!decrypt_page && IsEqualGUID(format, Gdiplus::ImageFormatJPEG)));
    if (!supported) {
        show_status(decrypt_page ? L"请选择有效的置乱 PNG。" : L"请选择有效的 PNG 或 JPEG。", true);
        return;
    }
    Image image;
    if (copy_bitmap(bitmap, image)) use_input(std::move(image), filename + dialog.nFileOffset);
}

void save_image() {
    auto &page = current();
    if (!page.output) return;
    const auto [a, b, rounds] = page.key;
    wchar_t filename[32768];
    swprintf(filename, ARRAYSIZE(filename), L"pixelloop-%ls-a%d-b%d-k%d.png",
             decrypt_page ? L"restored" : L"scrambled", a, b, rounds);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"无损 PNG\0*.png\0\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = ARRAYSIZE(filename);
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetSaveFileNameW(&dialog)) return;

    constexpr CLSID png_encoder = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0, 0, 0xf8, 0x1e, 0xf3, 0x2e}};
    const int size = page.output.size;
    Bitmap bitmap(size, size, size * 4, PixelFormat32bppARGB, reinterpret_cast<BYTE *>(page.output.pixels.data()));
    if (bitmap.Save(filename, &png_encoder, nullptr) == Ok) {
        show_status(L"已保存无损 PNG。");
    } else {
        show_status(L"无法写入 PNG，请检查保存位置。", true);
    }
}

void run_transform() {
    auto &page = current();
    if (!page.input) return;
    ArnoldKey key;
    if (!read_key(key)) {
        show_status(L"a 和 b 须在 1 到 99 之间，k 须在 1 到 9999 之间。", true);
        return;
    }
    page.key = key;
    page.output = {page.input.size, std::vector<uint32_t>(page.input.pixels.size())};
    arnold_transform(page.input.pixels.data(), page.output.pixels.data(), page.input.size, key, decrypt_page);
    if (!decrypt_page) {
        show_status(page.input.pixels == page.output.pixels
            ? L"这组参数和轮数下结果与原图相同，可尝试调整参数。"
            : L"置乱完成。保存 PNG 时会在文件名中记录参数。");
    } else if (page.reference) {
        if (page.reference.pixels == page.output.pixels) {
            const std::wstring message = L"恢复成功，与原图逐像素一致（" +
                std::to_wstring(page.output.pixels.size()) + L" 个像素）。";
            show_status(message.c_str());
        } else {
            show_status(L"结果与原图不同，请检查 a、b 和轮数 k。", true);
        }
    } else {
        show_status(L"已应用逆变换。可将结果与原图进行比较。");
    }
    refresh_result();
}

void transfer() {
    const auto &source = pages[0];
    if (!source.output) return;
    auto &target = pages[1];
    target = {};
    target.input = source.output;
    target.reference = source.input;
    target.key = source.key;
    target.name = L"来自本次加密的置乱图";
    target.status = L"已带入参数。运行解密后会与原图逐像素比较。";
    decrypt_page = true;
    refresh_page();
}

HWND control(const wchar_t *type, const wchar_t *text, DWORD style = 0, int id = 0, DWORD ex = 0) {
    HWND child = CreateWindowExW(ex, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1,
                                 window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(body_font), FALSE);
    return child;
}

void controls() {
    body_font = CreateFontW(-px(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
    title_font = CreateFontW(-px(30), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
    brand = control(L"STATIC", L"像素回环  /  PixelLoop");
    heading = control(L"STATIC", L"");
    SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(title_font), FALSE);
    hint = control(L"STATIC", L"");
    encrypt_button = control(L"BUTTON", L"加密", WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON | BS_PUSHLIKE, Encrypt);
    decrypt_button = control(L"BUTTON", L"解密", BS_AUTORADIOBUTTON | BS_PUSHLIKE, Decrypt);
    input_label = control(L"STATIC", L"", WS_GROUP);
    output_label = control(L"STATIC", L"");
    open_button = control(L"BUTTON", L"", WS_TABSTOP, Open);
    sample_button = control(L"BUTTON", L"使用示例图", WS_TABSTOP, Sample);
    save_button = control(L"BUTTON", L"保存 PNG", WS_TABSTOP, Save);
    transfer_button = control(L"BUTTON", L"转到解密", WS_TABSTOP, Transfer);
    file_label = control(L"STATIC", L"", SS_ENDELLIPSIS);
    parameters_label = control(L"STATIC", L"变换参数");
    a_label = control(L"STATIC", L"参数 a");
    b_label = control(L"STATIC", L"参数 b");
    rounds_label = control(L"STATIC", L"轮数 k");
    edit_a = control(L"EDIT", L"1", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, ParamA, WS_EX_CLIENTEDGE);
    edit_b = control(L"EDIT", L"1", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, ParamB, WS_EX_CLIENTEDGE);
    edit_rounds = control(L"EDIT", L"1", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, Rounds, WS_EX_CLIENTEDGE);
    SendMessageW(edit_a, EM_SETLIMITTEXT, 2, 0);
    SendMessageW(edit_b, EM_SETLIMITTEXT, 2, 0);
    SendMessageW(edit_rounds, EM_SETLIMITTEXT, 4, 0);
    matrix_label = control(L"STATIC", L"");
    run_button = control(L"BUTTON", L"", WS_TABSTOP | BS_DEFPUSHBUTTON, Run);
    condition_label = control(L"STATIC", L"");
    status_label = control(L"STATIC", L"", SS_ENDELLIPSIS);
}

void layout(int width, int height) {
    const int left = px(24), gap = px(16), side_width = px(244);
    const int side = width - left - side_width;
    const int pane_width = (side - left - gap*2) / 2;
    const int second = left + pane_width + gap;
    const int top = px(160), action = height - px(108);
    input_rect = {left, top, left + pane_width, action - px(12)};
    output_rect = {second, top, second + pane_width, action - px(12)};

    auto place = [](HWND handle, int x, int y, int w, int h) { MoveWindow(handle, x, y, w, h, TRUE); };
    place(brand, left, px(24), px(280), px(24));
    place(encrypt_button, width - left - px(164), px(16), px(78), px(36));
    place(decrypt_button, width - left - px(78), px(16), px(78), px(36));
    place(heading, left, px(62), side - left, px(42));
    place(hint, left, px(104), width - left*2, px(24));
    place(input_label, left, px(135), pane_width, px(24));
    place(output_label, second, px(135), pane_width, px(24));
    place(open_button, left, action, px(108), px(33));
    place(sample_button, left + px(114), action, px(100), px(33));
    const int actions_right = output_rect.right;
    place(transfer_button, actions_right - px(90), action, px(90), px(33));
    place(save_button, actions_right - px(decrypt_page ? 96 : 192), action, px(96), px(33));
    place(file_label, left, action + px(38), actions_right - left, px(24));
    place(status_label, left, height - px(39), width - left*2, px(28));

    place(parameters_label, side, px(136), side_width, px(26));
    place(a_label, side, px(174), px(70), px(23));
    place(b_label, side + px(82), px(174), px(70), px(23));
    place(rounds_label, side + px(164), px(174), px(80), px(23));
    place(edit_a, side, px(200), px(72), px(29));
    place(edit_b, side + px(82), px(200), px(72), px(29));
    place(edit_rounds, side + px(164), px(200), px(80), px(29));
    place(matrix_label, side, px(246), side_width, px(76));
    place(run_button, side, px(328), side_width, px(38));
    place(condition_label, side, px(381), side_width, px(76));
    InvalidateRect(window, nullptr, TRUE);
}

void paint_preview(HDC dc, const RECT &box, const Image &image, const wchar_t *placeholder) {
    HBRUSH fill = CreateSolidBrush(RGB(222, 232, 239));
    FillRect(dc, &box, fill);
    DeleteObject(fill);
    if (!image) {
        RECT area = box;
        SelectObject(dc, body_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(67, 87, 104));
        DrawTextW(dc, placeholder, -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    const int width = box.right - box.left, height = box.bottom - box.top;
    const int side = std::min(width, height);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = image.size;
    info.bmiHeader.biHeight = -image.size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, box.left + (width - side)/2, box.top + (height - side)/2, side, side,
                  0, 0, image.size, image.size, image.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK window_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            window = handle;
            controls();
            pages[1].status = L"打开置乱 PNG，或从加密页转入图像。";
            refresh_page();
            load_sample();
            return 0;
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED) layout(LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_GETMINMAXINFO: {
            auto *limits = reinterpret_cast<MINMAXINFO *>(lparam);
            limits->ptMinTrackSize = {px(818), px(575)};
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, current().error && reinterpret_cast<HWND>(lparam) == status_label
                ? RGB(147, 38, 29) : RGB(23, 48, 66));
            return reinterpret_cast<LRESULT>(background);
        }
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(handle, &paint);
            paint_preview(dc, input_rect, current().input, L"打开方形图像");
            paint_preview(dc, output_rect, current().output, L"运行后显示结果");
            EndPaint(handle, &paint);
            return 0;
        }
        case WM_COMMAND: {
            const WORD id = LOWORD(wparam);
            if (id == ParamA || id == ParamB || id == Rounds) {
                if (HIWORD(wparam) == EN_CHANGE && !filling_fields && edit_rounds) {
                    auto &page = current();
                    ArnoldKey key;
                    if (read_key(key)) page.key = key;
                    if (page.output) {
                        page.output = {};
                        show_status(L"参数已更改，请重新运行。");
                    }
                    refresh_matrix();
                    refresh_result();
                }
                return 0;
            }
            if (HIWORD(wparam) != BN_CLICKED) return 0;
            switch (id) {
                case Encrypt:
                case Decrypt:
                    decrypt_page = id == Decrypt;
                    refresh_page();
                    RECT bounds;
                    GetClientRect(window, &bounds);
                    layout(bounds.right, bounds.bottom);
                    break;
                case Open: open_image(); break;
                case Sample: load_sample(); break;
                case Save: save_image(); break;
                case Transfer: {
                    transfer();
                    RECT client;
                    GetClientRect(window, &client);
                    layout(client.right, client.bottom);
                    SetFocus(run_button);
                    break;
                }
                case Run: run_transform(); break;
            }
            return 0;
        }
        case WM_DESTROY:
            DeleteObject(body_font);
            DeleteObject(title_font);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(handle, message, wparam, lparam);
}
}

int WINAPI wWinMain(HINSTANCE module, HINSTANCE, PWSTR, int show) {
    instance = module;
    SetProcessDPIAware();
    HDC screen = GetDC(nullptr);
    if (screen) {
        dpi = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(nullptr, screen);
    }
    ULONG_PTR graphics_token = 0;
    Gdiplus::GdiplusStartupInput graphics_input;
    if (Gdiplus::GdiplusStartup(&graphics_token, &graphics_input, nullptr) != Ok) {
        MessageBoxW(nullptr, L"Windows 图像服务无法启动。", L"像素回环", MB_OK | MB_ICONERROR);
        return 1;
    }

    background = CreateSolidBrush(RGB(244, 248, 251));
    WNDCLASSEXW type{};
    type.cbSize = sizeof(type);
    type.hInstance = module;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    type.hbrBackground = background;
    type.lpfnWndProc = window_proc;
    type.lpszClassName = L"PixelLoop.Main";
    HWND main = nullptr;
    if (RegisterClassExW(&type)) {
        main = CreateWindowExW(WS_EX_CONTROLPARENT, type.lpszClassName, L"像素回环 PixelLoop",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
            px(1070), px(700), nullptr, nullptr, module, nullptr);
    }
    if (!main) {
        DeleteObject(background);
        Gdiplus::GdiplusShutdown(graphics_token);
        return 1;
    }

    ShowWindow(main, show);
    UpdateWindow(main);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(main, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    UnregisterClassW(type.lpszClassName, module);
    DeleteObject(background);
    Gdiplus::GdiplusShutdown(graphics_token);
    return 0;
}
