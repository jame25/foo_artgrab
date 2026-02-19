#include "stdafx.h"
#include "artgrab_preview.h"
#include <sstream>
#include <algorithm>
#include <dwmapi.h>
#include <windowsx.h>

#pragma comment(lib, "dwmapi.lib")

namespace artgrab {

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

PreviewWindow::PreviewWindow(const artwork_entry& entry)
    : m_image(nullptr)
    , m_image_width(0)
    , m_image_height(0)
    , m_fit_to_window(true)
    , m_pan_offset_x(0)
    , m_pan_offset_y(0)
    , m_is_dragging(false)
    , m_drag_start_pan_x(0)
    , m_drag_start_pan_y(0)
{
    SetRect(&m_image_rect, 0, 0, 0, 0);
    m_drag_start = { 0, 0 };

    // Load image from entry data via SHCreateMemStream
    if (entry.data.get_size() > 0) {
        IStream* stream = SHCreateMemStream(entry.data.get_ptr(), (UINT)entry.data.get_size());
        if (stream) {
            m_image = std::make_unique<Gdiplus::Image>(stream);
            stream->Release();

            if (m_image && m_image->GetLastStatus() == Gdiplus::Ok) {
                m_image_width = (int)m_image->GetWidth();
                m_image_height = (int)m_image->GetHeight();
            } else {
                m_image.reset();
            }
        }
    }

    // Build title: "{source} - {width}x{height}"
    // Convert source from UTF-8 to wide string
    const char* src = entry.source.get_ptr();
    int len = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    std::wstring wSource(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) MultiByteToWideChar(CP_UTF8, 0, src, -1, &wSource[0], len);

    std::wostringstream ss;
    ss << wSource << L" \u2014 " << m_image_width << L"x" << m_image_height;
    m_title = ss.str();
}

PreviewWindow::~PreviewWindow()
{
}

// ---------------------------------------------------------------------------
// ShowPopup
// ---------------------------------------------------------------------------

void PreviewWindow::ShowPopup(HWND parent)
{
    if (!m_image) return;

    // Calculate initial window size based on image, clamped to 80% of screen
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int max_width = (int)(screen_width * 0.8);
    int max_height = (int)(screen_height * 0.8);

    int window_width = m_image_width;
    int window_height = m_image_height;

    // Scale down if needed, maintaining aspect ratio
    if (window_width > max_width) {
        double scale = (double)max_width / window_width;
        window_width = max_width;
        window_height = (int)(window_height * scale);
    }
    if (window_height > max_height) {
        double scale = (double)max_height / window_height;
        window_height = max_height;
        window_width = (int)(window_width * scale);
    }

    // Ensure minimum size
    if (window_width < 200) window_width = 200;
    if (window_height < 200) window_height = 200;

    // Account for window frame
    RECT rc = { 0, 0, window_width, window_height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    window_width = rc.right - rc.left;
    window_height = rc.bottom - rc.top;

    // Create the window (non-modal, no parent ownership for independent windows)
    HWND hwnd = Create(NULL, CWindow::rcDefault, m_title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);

    if (hwnd) {
        ::SetWindowPos(hwnd, HWND_TOP, 0, 0, window_width, window_height, SWP_NOMOVE);
        CenterOnParent(parent);
        ShowWindow(SW_SHOW);
        SetFocus();
    }
}

// ---------------------------------------------------------------------------
// Message Handlers
// ---------------------------------------------------------------------------

LRESULT PreviewWindow::OnCreate(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    // Remove the icon from the titlebar
    SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)NULL);
    SendMessage(WM_SETICON, ICON_BIG, (LPARAM)NULL);
    LONG_PTR exStyle = ::GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);
    ::SetWindowLongPtr(m_hWnd, GWL_EXSTYLE, exStyle | WS_EX_DLGMODALFRAME);
    ::SetWindowPos(m_hWnd, NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // Dark mode title bar detection
    bool should_use_dark_titlebar = false;

    HKEY hkey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegQueryValueExA(hkey, "AppsUseLightTheme", NULL, NULL,
                (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            should_use_dark_titlebar = (value == 0);
        }
        RegCloseKey(hkey);
    }

    // Also check foobar2000 background as hint
    COLORREF bg_color = GetSysColor(COLOR_WINDOW);
    try {
        ui_config_manager::ptr ui_config = ui_config_manager::tryGet();
        if (ui_config.is_valid()) {
            t_ui_color fb2k_bg_color;
            if (ui_config->query_color(ui_color_background, fb2k_bg_color)) {
                bg_color = fb2k_bg_color;
            }
        }
    } catch (...) {}

    BYTE r = GetRValue(bg_color);
    BYTE g = GetGValue(bg_color);
    BYTE b = GetBValue(bg_color);
    int brightness = (r * 299 + g * 587 + b * 114) / 1000;
    if (brightness < 128) {
        should_use_dark_titlebar = true;
    }

    if (should_use_dark_titlebar) {
        BOOL dark_mode = TRUE;
        // DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 11 / Windows 10 20H1+)
        if (FAILED(DwmSetWindowAttribute(m_hWnd, 20, &dark_mode, sizeof(dark_mode)))) {
            DwmSetWindowAttribute(m_hWnd, 19, &dark_mode, sizeof(dark_mode));
        }
    }

    bHandled = TRUE;
    return 0;
}

LRESULT PreviewWindow::OnDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    bHandled = FALSE;
    return 0;
}

LRESULT PreviewWindow::OnNcDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    // Self-delete: the window was allocated with new
    bHandled = FALSE;
    delete this;
    return 0;
}

LRESULT PreviewWindow::OnPaint(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(&ps);

    RECT client;
    GetClientRect(&client);
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;

    if (cw <= 0 || ch <= 0) {
        EndPaint(&ps);
        bHandled = TRUE;
        return 0;
    }

    // Double-buffered drawing
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, cw, ch);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Dark gray background for gallery feel
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);

    // Draw the image
    if (m_image) {
        Gdiplus::Graphics graphics(memDC);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        if (m_fit_to_window) {
            // Fit mode: draw scaled to m_image_rect
            Gdiplus::Rect destRect(
                m_image_rect.left, m_image_rect.top,
                m_image_rect.right - m_image_rect.left,
                m_image_rect.bottom - m_image_rect.top);
            graphics.DrawImage(m_image.get(), destRect);
        } else {
            // Original size mode: draw at 1:1 with pan offset, clipped to client
            int dest_left = (m_image_rect.left < 0) ? 0 : m_image_rect.left;
            int dest_top = (m_image_rect.top < 0) ? 0 : m_image_rect.top;
            int dest_right = (m_image_rect.right > cw) ? cw : m_image_rect.right;
            int dest_bottom = (m_image_rect.bottom > ch) ? ch : m_image_rect.bottom;

            int dest_width = dest_right - dest_left;
            int dest_height = dest_bottom - dest_top;

            if (dest_width > 0 && dest_height > 0) {
                int src_left = dest_left - m_image_rect.left;
                int src_top = dest_top - m_image_rect.top;
                int src_width = dest_width;
                int src_height = dest_height;

                // Clamp source to image bounds
                if (src_left < 0) src_left = 0;
                if (src_top < 0) src_top = 0;
                if (src_left + src_width > m_image_width)
                    src_width = m_image_width - src_left;
                if (src_top + src_height > m_image_height)
                    src_height = m_image_height - src_top;

                if (src_width > 0 && src_height > 0) {
                    Gdiplus::Rect destRect(dest_left, dest_top, src_width, src_height);
                    graphics.DrawImage(m_image.get(), destRect,
                        src_left, src_top, src_width, src_height,
                        Gdiplus::UnitPixel);
                }
            }
        }
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, cw, ch, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(&ps);
    bHandled = TRUE;
    return 0;
}

LRESULT PreviewWindow::OnSize(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    // Clamp pan offset after resize
    if (m_image && !m_fit_to_window) {
        RECT client;
        GetClientRect(&client);
        int available_width = client.right - client.left;
        int available_height = client.bottom - client.top;

        int max_pan_x = 0, min_pan_x = 0, max_pan_y = 0, min_pan_y = 0;
        if (m_image_width > available_width) {
            int excess = m_image_width - available_width;
            max_pan_x = excess / 2;
            min_pan_x = -excess / 2 - (excess % 2);
        }
        if (m_image_height > available_height) {
            int excess = m_image_height - available_height;
            max_pan_y = excess / 2;
            min_pan_y = -excess / 2 - (excess % 2);
        }

        if (m_pan_offset_x > max_pan_x) m_pan_offset_x = max_pan_x;
        if (m_pan_offset_x < min_pan_x) m_pan_offset_x = min_pan_x;
        if (m_pan_offset_y > max_pan_y) m_pan_offset_y = max_pan_y;
        if (m_pan_offset_y < min_pan_y) m_pan_offset_y = min_pan_y;
    }

    CalculateImageRect();
    Invalidate();
    bHandled = TRUE;
    return 0;
}

LRESULT PreviewWindow::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    bHandled = TRUE;
    return TRUE;
}

LRESULT PreviewWindow::OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
    if (wParam == VK_ESCAPE) {
        PostMessage(WM_CLOSE);
    }
    bHandled = TRUE;
    return 0;
}

LRESULT PreviewWindow::OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    // Toggle between fit-to-window and original size
    m_fit_to_window = !m_fit_to_window;
    m_pan_offset_x = 0;
    m_pan_offset_y = 0;
    CalculateImageRect();
    Invalidate();
    bHandled = TRUE;
    return 0;
}

LRESULT PreviewWindow::OnLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
    // Start drag for panning in original-size mode
    if (!m_fit_to_window && m_image) {
        RECT client;
        GetClientRect(&client);
        int available_width = client.right - client.left;
        int available_height = client.bottom - client.top;

        if (m_image_width > available_width || m_image_height > available_height) {
            m_is_dragging = true;
            m_drag_start.x = GET_X_LPARAM(lParam);
            m_drag_start.y = GET_Y_LPARAM(lParam);
            m_drag_start_pan_x = m_pan_offset_x;
            m_drag_start_pan_y = m_pan_offset_y;
            SetCapture();
            bHandled = TRUE;
            return 0;
        }
    }
    bHandled = FALSE;
    return 0;
}

LRESULT PreviewWindow::OnLButtonUp(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    if (m_is_dragging) {
        m_is_dragging = false;
        ReleaseCapture();
        bHandled = TRUE;
        return 0;
    }
    bHandled = FALSE;
    return 0;
}

LRESULT PreviewWindow::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
    if (m_is_dragging && m_image) {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        int delta_x = pt.x - m_drag_start.x;
        int delta_y = pt.y - m_drag_start.y;

        int new_pan_x = m_drag_start_pan_x + delta_x;
        int new_pan_y = m_drag_start_pan_y + delta_y;

        // Calculate pan bounds
        RECT client;
        GetClientRect(&client);
        int available_width = client.right - client.left;
        int available_height = client.bottom - client.top;

        int max_pan_x = 0, min_pan_x = 0, max_pan_y = 0, min_pan_y = 0;

        if (m_image_width > available_width) {
            int excess = m_image_width - available_width;
            max_pan_x = excess / 2;
            min_pan_x = -excess / 2 - (excess % 2);
        }
        if (m_image_height > available_height) {
            int excess = m_image_height - available_height;
            max_pan_y = excess / 2;
            min_pan_y = -excess / 2 - (excess % 2);
        }

        // Clamp
        if (new_pan_x > max_pan_x) new_pan_x = max_pan_x;
        if (new_pan_x < min_pan_x) new_pan_x = min_pan_x;
        if (new_pan_y > max_pan_y) new_pan_y = max_pan_y;
        if (new_pan_y < min_pan_y) new_pan_y = min_pan_y;

        if (new_pan_x != m_pan_offset_x || new_pan_y != m_pan_offset_y) {
            m_pan_offset_x = new_pan_x;
            m_pan_offset_y = new_pan_y;
            CalculateImageRect();
            Invalidate(FALSE);
        }

        bHandled = TRUE;
        return 0;
    }
    bHandled = FALSE;
    return 0;
}

LRESULT PreviewWindow::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
    MINMAXINFO* mmi = (MINMAXINFO*)lParam;
    mmi->ptMinTrackSize.x = 200;
    mmi->ptMinTrackSize.y = 200;
    bHandled = TRUE;
    return 0;
}

LRESULT PreviewWindow::OnClose(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    DestroyWindow();
    bHandled = TRUE;
    return 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void PreviewWindow::CalculateImageRect()
{
    if (!m_image) return;

    RECT client;
    GetClientRect(&client);
    int available_width = client.right - client.left;
    int available_height = client.bottom - client.top;

    if (available_width <= 0 || available_height <= 0) return;

    int draw_width, draw_height, draw_x, draw_y;

    if (m_fit_to_window) {
        // Fit to window maintaining aspect ratio
        double img_aspect = (double)m_image_width / m_image_height;
        double avail_aspect = (double)available_width / available_height;

        if (img_aspect > avail_aspect) {
            draw_width = available_width;
            draw_height = (int)(available_width / img_aspect);
        } else {
            draw_height = available_height;
            draw_width = (int)(available_height * img_aspect);
        }

        draw_x = (available_width - draw_width) / 2;
        draw_y = (available_height - draw_height) / 2;
    } else {
        // Original size, centered with pan offset
        draw_width = m_image_width;
        draw_height = m_image_height;
        draw_x = (available_width - m_image_width) / 2 + m_pan_offset_x;
        draw_y = (available_height - m_image_height) / 2 + m_pan_offset_y;
    }

    SetRect(&m_image_rect, draw_x, draw_y, draw_x + draw_width, draw_y + draw_height);
}

void PreviewWindow::CenterOnParent(HWND parent)
{
    RECT parent_rect, window_rect;

    if (parent && ::GetWindowRect(parent, &parent_rect)) {
        ::GetWindowRect(m_hWnd, &window_rect);

        int parent_width = parent_rect.right - parent_rect.left;
        int parent_height = parent_rect.bottom - parent_rect.top;
        int window_width = window_rect.right - window_rect.left;
        int window_height = window_rect.bottom - window_rect.top;

        int x = parent_rect.left + (parent_width - window_width) / 2;
        int y = parent_rect.top + (parent_height - window_height) / 2;

        ::SetWindowPos(m_hWnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    } else {
        // Center on screen
        int screen_width = GetSystemMetrics(SM_CXSCREEN);
        int screen_height = GetSystemMetrics(SM_CYSCREEN);

        ::GetWindowRect(m_hWnd, &window_rect);
        int window_width = window_rect.right - window_rect.left;
        int window_height = window_rect.bottom - window_rect.top;

        int x = (screen_width - window_width) / 2;
        int y = (screen_height - window_height) / 2;

        ::SetWindowPos(m_hWnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    }
}

} // namespace artgrab
