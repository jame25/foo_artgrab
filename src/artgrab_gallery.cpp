#include "stdafx.h"
#include "artgrab_gallery.h"
#include "artgrab_entry.h"
#include "artgrab_preview.h"
#include "artgrab_utils.h"
#include "async_io_manager.h"
#include <algorithm>
#include <sstream>
#include <cmath>

#include "artgrab_preferences.h"

static bool is_back_cover(const artgrab::artwork_entry& entry) {
    return strstr(entry.source.get_ptr(), "(Back)") != nullptr;
}

static bool is_artist_image(const artgrab::artwork_entry& entry) {
    return strstr(entry.source.get_ptr(), "(Artist)") != nullptr;
}

static std::string replace_extension(const std::string& filename, const char* new_ext) {
    size_t dot = filename.rfind('.');
    if (dot != std::string::npos) {
        return filename.substr(0, dot) + new_ext;
    }
    return filename + new_ext;
}

// Strip content in parentheses and square brackets, e.g. "(Remastered)", "[Deluxe Edition]"
static std::string strip_brackets(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    int paren = 0, square = 0;
    for (char c : s) {
        if (c == '(') { paren++; continue; }
        if (c == ')') { if (paren > 0) paren--; continue; }
        if (c == '[') { square++; continue; }
        if (c == ']') { if (square > 0) square--; continue; }
        if (paren == 0 && square == 0) result += c;
    }
    // Trim trailing whitespace left behind
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

namespace artgrab {

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

GalleryWindow::GalleryWindow(const char* artist, const char* album, const char* file_path)
    : m_artist(artist ? artist : "")
    , m_album(album ? album : "")
    , m_file_path(file_path ? file_path : "")
    , m_selected_index(-1)
    , m_all_done(false)
    , m_scroll_y(0)
    , m_content_height(0)
    , m_save_button(NULL)
    , m_artist_label(NULL)
    , m_album_label(NULL)
    , m_artist_edit(NULL)
    , m_album_edit(NULL)
    , m_search_button(NULL)
    , m_search_panel_open(false)
    , m_search_anim_t(0.0f)
    , m_search_anim_opening(false)
{
}

GalleryWindow::~GalleryWindow()
{
    if (m_search) {
        m_search->cancel();
    }
}

// ---------------------------------------------------------------------------
// Show
// ---------------------------------------------------------------------------

void GalleryWindow::Show(HWND parent)
{
    RECT rc = {0, 0, 680, 520};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, FALSE);
    Create(parent, &rc, L"Artwork Grabber", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
    CenterWindow(parent);
    ShowWindow(SW_SHOW);
    UpdateWindow();
}

// ---------------------------------------------------------------------------
// Message Handlers
// ---------------------------------------------------------------------------

LRESULT GalleryWindow::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    // Remove the icon from the titlebar
    SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)NULL);
    SendMessage(WM_SETICON, ICON_BIG, (LPARAM)NULL);
    LONG_PTR exStyle = ::GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);
    ::SetWindowLongPtr(m_hWnd, GWL_EXSTYLE, exStyle | WS_EX_DLGMODALFRAME);
    ::SetWindowPos(m_hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // Derive album folder from file path
    pfc::string8 dir = artgrab::get_file_directory(m_file_path.c_str());
    m_album_folder = dir.get_ptr();

    // Create Save button
    m_save_button = ::CreateWindowW(
        L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 28,
        m_hWnd, (HMENU)(INT_PTR)ID_SAVE_BUTTON,
        NULL, NULL);

    // Disable Save until a selection is made
    ::EnableWindow(m_save_button, FALSE);

    // Convert artist/album to wide strings for edit controls
    auto to_wide = [](const std::string& s) -> std::wstring {
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        std::wstring ws(len > 0 ? len - 1 : 0, L'\0');
        if (len > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
        return ws;
    };

    std::wstring wArtist = to_wide(m_artist);
    std::wstring wAlbum = to_wide(m_album);

    // Create search panel controls (hidden initially)
    m_artist_label = ::CreateWindowW(L"BUTTON", L"",
        WS_CHILD | BS_OWNERDRAW,
        0, 0, 24, 24, m_hWnd, (HMENU)(INT_PTR)ID_ARTIST_LABEL, NULL, NULL);

    m_artist_edit = ::CreateWindowW(L"EDIT", wArtist.c_str(),
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 200, 24, m_hWnd, (HMENU)(INT_PTR)ID_ARTIST_EDIT, NULL, NULL);

    m_album_label = ::CreateWindowW(L"BUTTON", L"",
        WS_CHILD | BS_OWNERDRAW,
        0, 0, 24, 24, m_hWnd, (HMENU)(INT_PTR)ID_ALBUM_LABEL, NULL, NULL);

    m_album_edit = ::CreateWindowW(L"EDIT", wAlbum.c_str(),
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 200, 24, m_hWnd, (HMENU)(INT_PTR)ID_ALBUM_EDIT, NULL, NULL);

    m_search_button = ::CreateWindowW(L"BUTTON", L"",
        WS_CHILD | BS_OWNERDRAW,
        0, 0, SEARCH_ICON_SIZE, 24, m_hWnd, (HMENU)(INT_PTR)ID_SEARCH_BUTTON, NULL, NULL);

    // If no album folder, permanently disable save
    if (m_album_folder.empty()) {
        m_status_text = L"No album folder available";
    }

    InitApiStates();

    if (m_status_text.empty()) {
        m_status_text = L"Searching...";
    }

    StartSearch();
    RecalcLayout();

    return 0;
}

LRESULT GalleryWindow::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    KillTimer(SEARCH_ANIM_TIMER_ID);
    RemoveKeyboardHook();
    if (m_search) {
        m_search->cancel();
        m_search.reset();
    }
    m_cells.clear();
    return 0;
}

LRESULT GalleryWindow::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    if (m_search) {
        m_search->cancel();
    }
    HWND owner = GetWindow(GW_OWNER);
    if (owner && ::IsWindow(owner)) {
        ::SetForegroundWindow(owner);
    }
    DestroyWindow();
    return 0;
}

LRESULT GalleryWindow::OnNcDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Must set bHandled = TRUE so ATL doesn't access 'this' after delete.
    // We call DefWindowProc and clean up m_hWnd ourselves.
    bHandled = TRUE;
    LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
    m_hWnd = NULL;
    delete this;
    return lRes;
}

LRESULT GalleryWindow::OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    return 1; // We handle erasing in OnPaint (double-buffered)
}

LRESULT GalleryWindow::OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
    MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
    mmi->ptMinTrackSize.x = MIN_WIDTH;
    mmi->ptMinTrackSize.y = MIN_HEIGHT;
    return 0;
}

LRESULT GalleryWindow::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    RecalcLayout();
    InvalidateRect(NULL, FALSE);
    return 0;
}

LRESULT GalleryWindow::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(&ps);

    RECT client;
    GetClientRect(&client);
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;

    if (cw <= 0 || ch <= 0) {
        EndPaint(&ps);
        return 0;
    }

    // Double-buffered drawing
    Gdiplus::Bitmap backBuffer(cw, ch, PixelFormat32bppARGB);
    Gdiplus::Graphics g(&backBuffer);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // Background
    COLORREF bgColor = GetSysColor(COLOR_WINDOW);
    Gdiplus::SolidBrush bgBrush(Gdiplus::Color(GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor)));
    g.FillRectangle(&bgBrush, 0, 0, cw, ch);

    // Paint the thumbnail grid
    PaintGrid(g);

    // Paint floating search panel background (if open or animating)
    if (m_search_anim_t > 0.0f) {
        COLORREF faceBg = GetSysColor(COLOR_3DFACE);
        Gdiplus::SolidBrush panelBrush(Gdiplus::Color(230,
            GetRValue(faceBg), GetGValue(faceBg), GetBValue(faceBg)));
        Gdiplus::Pen panelBorder(Gdiplus::Color(160, 160, 160));
        int panel_y = ch - STATUS_BAR_HEIGHT - SEARCH_PANEL_HEIGHT - SEARCH_ICON_MARGIN;

        // Calculate panel width (matching RecalcLayout)
        int icon_label_w = 24;
        int icon_btn_w = SEARCH_ICON_SIZE;
        int gap = 4;
        int margin = SEARCH_ICON_MARGIN;
        int max_panel_w = cw * 85 / 100;
        int fixed_w = icon_label_w + gap + icon_label_w + gap + icon_btn_w + gap * 2 + margin;
        int edit_total = max_panel_w - fixed_w;
        int edit_w = (std::max)(60, edit_total / 2);
        int total_content_w = icon_label_w + gap + edit_w + gap + icon_label_w + gap + edit_w + gap + icon_btn_w + margin;

        // Ease-out for background too
        float eased = 1.0f - (1.0f - m_search_anim_t) * (1.0f - m_search_anim_t);
        int panel_w = (int)(total_content_w * eased);
        int panel_x = cw - panel_w;

        Gdiplus::Rect panelRect(panel_x, panel_y, panel_w, SEARCH_PANEL_HEIGHT + SEARCH_ICON_MARGIN);
        g.FillRectangle(&panelBrush, panelRect);
        g.DrawLine(&panelBorder, panel_x, panel_y, cw, panel_y);
    }

    // Paint search icon overlay
    PaintSearchIcon(g);

    // Paint status bar
    PaintStatusBar(g, client);

    // Blit to screen
    Gdiplus::Graphics screenG(hdc);
    screenG.DrawImage(&backBuffer, 0, 0);

    EndPaint(&ps);
    return 0;
}

LRESULT GalleryWindow::OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

    // Check if search icon was clicked (bottom-right corner)
    RECT client;
    GetClientRect(&client);
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;
    int icon_x = cw - SEARCH_ICON_SIZE - SEARCH_ICON_MARGIN;
    int icon_y = ch - STATUS_BAR_HEIGHT - SEARCH_ICON_SIZE - SEARCH_ICON_MARGIN;
    RECT iconRect = { icon_x, icon_y,
                      icon_x + SEARCH_ICON_SIZE,
                      icon_y + SEARCH_ICON_SIZE };
    if (PtInRect(&iconRect, pt)) {
        ToggleSearchPanel();
        return 0;
    }

    int hit = HitTest(pt);

    if (hit >= 0) {
        // Deselect previous
        if (m_selected_index >= 0 && m_selected_index < (int)m_cells.size()) {
            m_cells[m_selected_index].selected = false;
        }
        m_selected_index = hit;
        m_cells[hit].selected = true;
        if (!m_album_folder.empty()) {
            ::EnableWindow(m_save_button, TRUE);
        }
    } else {
        // Deselect
        if (m_selected_index >= 0 && m_selected_index < (int)m_cells.size()) {
            m_cells[m_selected_index].selected = false;
        }
        m_selected_index = -1;
        ::EnableWindow(m_save_button, FALSE);
    }

    InvalidateRect(NULL, FALSE);
    return 0;
}

LRESULT GalleryWindow::OnLButtonDblClk(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    int hit = HitTest(pt);
    if (hit >= 0) {
        DoPreview(hit);
    }
    return 0;
}

LRESULT GalleryWindow::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    int scroll_amount = THUMB_SIZE;

    if (delta > 0) {
        m_scroll_y -= scroll_amount;
    } else {
        m_scroll_y += scroll_amount;
    }

    // Clamp scroll
    RECT client;
    GetClientRect(&client);
    int visible_height = (client.bottom - client.top) - STATUS_BAR_HEIGHT;
    int max_scroll = m_content_height - visible_height;
    if (max_scroll < 0) max_scroll = 0;
    if (m_scroll_y < 0) m_scroll_y = 0;
    if (m_scroll_y > max_scroll) m_scroll_y = max_scroll;

    RecalcLayout();
    InvalidateRect(NULL, FALSE);
    return 0;
}

LRESULT GalleryWindow::OnCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
    WORD id = LOWORD(wParam);
    if (id == ID_SEARCH_BUTTON) {
        ResetSearch();
        ShowSearchPanel(false);
        return 0;
    }
    if (id == ID_SAVE_BUTTON) {
        bool shift_held = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        DoSave(shift_held);
        return 0;
    }
    bHandled = FALSE;
    return 0;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void GalleryWindow::InitApiStates()
{
    m_api_states.clear();
    const char* all_apis[] = { "iTunes", "Deezer", "Last.fm", "MusicBrainz", "Discogs" };
    for (const char* api : all_apis) {
        if (artgrab::is_api_enabled(api)) {
            m_api_states.emplace_back(api);
        }
    }
    if (cfg_ag_include_back_covers) {
        if (artgrab::is_api_enabled("MusicBrainz"))
            m_api_states.emplace_back("MusicBrainz (Back)");
        if (artgrab::is_api_enabled("Discogs"))
            m_api_states.emplace_back("Discogs (Back)");
    }
    if (cfg_ag_include_artist_images) {
        if (artgrab::is_api_enabled("Deezer"))
            m_api_states.emplace_back("Deezer (Artist)");
    }
}

void GalleryWindow::StartSearch()
{
    HWND hwnd = m_hWnd;
    GalleryWindow* self = this;

    // Note: All artwork_search callbacks already arrive on the main thread
    // (marshalled by async_io_manager::post_to_main_thread in perform_http_get/perform_http_get_binary).
    // We guard with IsWindow(hwnd) to handle the case where the window was destroyed
    // while callbacks were in-flight.

    auto on_result_cb = [self, hwnd](const artwork_entry& entry) {
        if (!::IsWindow(hwnd)) return;

        ThumbnailCell cell;
        cell.entry = entry;
        cell.thumbnail = self->CreateThumbnail(cell.entry);
        cell.selected = false;
        self->m_cells.push_back(std::move(cell));
        self->RecalcLayout();
        self->UpdateStatusText();
        self->InvalidateRect(NULL, FALSE);
    };

    auto on_api_done_cb = [self, hwnd](const char* api_name, bool had_results) {
        if (!::IsWindow(hwnd)) return;

        for (auto& state : self->m_api_states) {
            if (state.name == api_name) {
                state.done = true;
                state.had_results = had_results;
                break;
            }
        }
        self->UpdateStatusText();
        self->InvalidateRect(NULL, FALSE);
    };

    auto on_all_done_cb = [self, hwnd]() {
        if (!::IsWindow(hwnd)) return;

        self->m_all_done = true;
        self->UpdateStatusText();
        self->InvalidateRect(NULL, FALSE);
    };

    int max_results = cfg_ag_max_results.get_value();
    if (max_results < 1) max_results = 3;

    m_search_artist = strip_brackets(m_artist);
    m_search_album = strip_brackets(m_album);

    m_search = std::make_shared<artwork_search>(
        m_search_artist.c_str(), m_search_album.c_str(), max_results,
        cfg_ag_include_back_covers.get_value(),
        cfg_ag_include_artist_images.get_value(),
        on_result_cb, on_api_done_cb, on_all_done_cb);
    m_search->start();
}

void GalleryWindow::ResetSearch()
{
    // Cancel in-flight search
    if (m_search) {
        m_search->cancel();
        m_search.reset();
    }

    // Clear results
    m_cells.clear();
    m_selected_index = -1;
    m_all_done = false;
    m_scroll_y = 0;
    m_content_height = 0;
    ::EnableWindow(m_save_button, FALSE);

    // Read new values from edit controls
    WCHAR buf[512];
    ::GetWindowTextW(m_artist_edit, buf, 512);
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
        m_artist.resize(len > 0 ? len - 1 : 0);
        if (len > 1) WideCharToMultiByte(CP_UTF8, 0, buf, -1, &m_artist[0], len, NULL, NULL);
    }

    ::GetWindowTextW(m_album_edit, buf, 512);
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
        m_album.resize(len > 0 ? len - 1 : 0);
        if (len > 1) WideCharToMultiByte(CP_UTF8, 0, buf, -1, &m_album[0], len, NULL, NULL);
    }

    // Rebuild API states
    InitApiStates();

    m_status_text = L"Searching...";
    StartSearch();
    RecalcLayout();
    InvalidateRect(NULL, FALSE);
}

void GalleryWindow::UpdateStatusText()
{
    if (m_all_done) {
        if (m_cells.empty()) {
            m_status_text = L"No artwork found for this album";
        } else {
            std::wostringstream ss;
            ss << L"Done \u2014 " << m_cells.size() << L" result" << (m_cells.size() != 1 ? L"s" : L"") << L" found";
            m_status_text = ss.str();
        }
    } else {
        int done_count = 0;
        for (const auto& s : m_api_states) {
            if (s.done) done_count++;
        }
        std::wostringstream ss;
        ss << L"Searching... (" << done_count << L"/" << m_api_states.size() << L" APIs done, "
           << m_cells.size() << L" result" << (m_cells.size() != 1 ? L"s" : L"") << L")";
        m_status_text = ss.str();
    }
}

LRESULT GalleryWindow::OnKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
    if (wParam == VK_ESCAPE && m_search_panel_open) {
        ShowSearchPanel(false);
        return 0;
    }
    bHandled = FALSE;
    return 0;
}

LRESULT GalleryWindow::OnDrawItem(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);

    // Common setup for all owner-drawn controls
    if (dis->CtlID == ID_SEARCH_BUTTON || dis->CtlID == ID_ARTIST_LABEL || dis->CtlID == ID_ALBUM_LABEL) {
        // Draw button background
        COLORREF btnBg = GetSysColor(dis->itemState & ODS_SELECTED ? COLOR_3DSHADOW : COLOR_3DFACE);
        HBRUSH hBrush = CreateSolidBrush(btnBg);
        FillRect(dis->hDC, &dis->rcItem, hBrush);
        DeleteObject(hBrush);

        // Only draw edge on the search button
        if (dis->CtlID == ID_SEARCH_BUTTON) {
            DrawEdge(dis->hDC, &dis->rcItem, dis->itemState & ODS_SELECTED ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
        }

        Gdiplus::Graphics g(dis->hDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        int w = dis->rcItem.right - dis->rcItem.left;
        int h = dis->rcItem.bottom - dis->rcItem.top;
        float ox = (float)(dis->rcItem.left + (w - 16) / 2);
        float oy = (float)(dis->rcItem.top + (h - 16) / 2);

        COLORREF textColor = GetSysColor(COLOR_BTNTEXT);
        Gdiplus::Color iconClr(GetRValue(textColor), GetGValue(textColor), GetBValue(textColor));

        if (dis->CtlID == ID_SEARCH_BUTTON) {
            // Search icon: magnifying glass
            float size = 16.0f;
            Gdiplus::Pen iconPen(iconClr, 1.5f);
            iconPen.SetStartCap(Gdiplus::LineCapRound);
            iconPen.SetEndCap(Gdiplus::LineCapRound);

            float cx = ox + size * 0.42f;
            float cy = oy + size * 0.42f;
            float r = size * 0.28f;
            g.DrawEllipse(&iconPen, cx - r, cy - r, r * 2, r * 2);

            float angle = 0.785f;
            float hx1 = cx + r * cosf(angle);
            float hy1 = cy + r * sinf(angle);
            float hx2 = ox + size * 0.88f;
            float hy2 = oy + size * 0.88f;
            g.DrawLine(&iconPen, hx1, hy1, hx2, hy2);
        }
        else if (dis->CtlID == ID_ARTIST_LABEL) {
            // Artist icon: person silhouette (head circle + body arc)
            Gdiplus::Pen iconPen(iconClr, 1.4f);
            Gdiplus::SolidBrush iconBrush(iconClr);

            // Head (circle)
            float headR = 3.2f;
            float headCx = ox + 8.0f;
            float headCy = oy + 4.0f;
            g.DrawEllipse(&iconPen, headCx - headR, headCy - headR, headR * 2, headR * 2);

            // Body (arc/shoulders)
            Gdiplus::GraphicsPath path;
            path.AddArc(ox + 1.5f, oy + 9.0f, 13.0f, 10.0f, 180.0f, 180.0f);
            g.DrawPath(&iconPen, &path);
        }
        else if (dis->CtlID == ID_ALBUM_LABEL) {
            // Album icon: vinyl disc (two concentric circles + center dot)
            Gdiplus::Pen iconPen(iconClr, 1.4f);
            Gdiplus::SolidBrush iconBrush(iconClr);

            float discCx = ox + 8.0f;
            float discCy = oy + 8.0f;

            // Outer circle (disc)
            float outerR = 7.0f;
            g.DrawEllipse(&iconPen, discCx - outerR, discCy - outerR, outerR * 2, outerR * 2);

            // Inner circle (label area)
            float innerR = 3.5f;
            g.DrawEllipse(&iconPen, discCx - innerR, discCy - innerR, innerR * 2, innerR * 2);

            // Center dot (spindle hole)
            float dotR = 1.2f;
            g.FillEllipse(&iconBrush, discCx - dotR, discCy - dotR, dotR * 2, dotR * 2);
        }

        return TRUE;
    }
    bHandled = FALSE;
    return 0;
}

// ---------------------------------------------------------------------------
// Keyboard hook for Enter/Tab in search edit controls
// ---------------------------------------------------------------------------

GalleryWindow* GalleryWindow::s_hook_instance = nullptr;
HHOOK GalleryWindow::s_keyboard_hook = NULL;

LRESULT CALLBACK GalleryWindow::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && !(lParam & 0x80000000) && s_hook_instance) {
        HWND focus = ::GetFocus();
        bool in_edit = (focus == s_hook_instance->m_artist_edit ||
                        focus == s_hook_instance->m_album_edit);
        if (in_edit) {
            if (wParam == VK_RETURN) {
                // Post to avoid re-entrancy issues with the hook
                ::PostMessage(s_hook_instance->m_hWnd, WM_COMMAND,
                    MAKEWPARAM(ID_SEARCH_BUTTON, BN_CLICKED), 0);
                return 1; // Eat the keystroke
            }
            if (wParam == VK_TAB) {
                // Toggle focus between the two edit fields
                HWND other = (focus == s_hook_instance->m_artist_edit)
                    ? s_hook_instance->m_album_edit
                    : s_hook_instance->m_artist_edit;
                ::SetFocus(other);
                return 1;
            }
        }
    }
    return ::CallNextHookEx(s_keyboard_hook, nCode, wParam, lParam);
}

void GalleryWindow::InstallKeyboardHook()
{
    if (!s_keyboard_hook) {
        s_hook_instance = this;
        s_keyboard_hook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardHookProc,
            NULL, ::GetCurrentThreadId());
    }
}

void GalleryWindow::RemoveKeyboardHook()
{
    if (s_keyboard_hook) {
        ::UnhookWindowsHookEx(s_keyboard_hook);
        s_keyboard_hook = NULL;
        s_hook_instance = nullptr;
    }
}

void GalleryWindow::ToggleSearchPanel()
{
    ShowSearchPanel(!m_search_panel_open);
}

void GalleryWindow::ShowSearchPanel(bool show)
{
    m_search_panel_open = show;
    m_search_anim_opening = show;

    if (show) {
        InstallKeyboardHook();
    } else {
        RemoveKeyboardHook();
    }

    // Don't show controls here — they'd appear at stale positions (0,0).
    // OnTimer will show them after RecalcLayout positions them on the first tick.

    // Start animation timer
    SetTimer(SEARCH_ANIM_TIMER_ID, SEARCH_ANIM_INTERVAL_MS);
}

LRESULT GalleryWindow::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
    if (wParam == SEARCH_ANIM_TIMER_ID) {
        bool was_zero = (m_search_anim_t == 0.0f);
        float step = 0.08f; // ~12 frames to complete
        if (m_search_anim_opening) {
            m_search_anim_t += step;
            if (m_search_anim_t >= 1.0f) {
                m_search_anim_t = 1.0f;
                KillTimer(SEARCH_ANIM_TIMER_ID);
            }
        } else {
            m_search_anim_t -= step;
            if (m_search_anim_t <= 0.0f) {
                m_search_anim_t = 0.0f;
                KillTimer(SEARCH_ANIM_TIMER_ID);
                // Hide controls once fully closed
                if (m_artist_label && ::IsWindow(m_artist_label)) ::ShowWindow(m_artist_label, SW_HIDE);
                if (m_artist_edit && ::IsWindow(m_artist_edit)) ::ShowWindow(m_artist_edit, SW_HIDE);
                if (m_album_label && ::IsWindow(m_album_label)) ::ShowWindow(m_album_label, SW_HIDE);
                if (m_album_edit && ::IsWindow(m_album_edit)) ::ShowWindow(m_album_edit, SW_HIDE);
                if (m_search_button && ::IsWindow(m_search_button)) ::ShowWindow(m_search_button, SW_HIDE);
            }
        }

        // Position controls first, then show them
        RecalcLayout();

        // Show controls on the first tick after RecalcLayout has positioned them
        if (was_zero && m_search_anim_opening) {
            if (m_artist_label && ::IsWindow(m_artist_label)) ::ShowWindow(m_artist_label, SW_SHOW);
            if (m_artist_edit && ::IsWindow(m_artist_edit)) ::ShowWindow(m_artist_edit, SW_SHOW);
            if (m_album_label && ::IsWindow(m_album_label)) ::ShowWindow(m_album_label, SW_SHOW);
            if (m_album_edit && ::IsWindow(m_album_edit)) ::ShowWindow(m_album_edit, SW_SHOW);
            if (m_search_button && ::IsWindow(m_search_button)) ::ShowWindow(m_search_button, SW_SHOW);
            if (m_artist_edit) ::SetFocus(m_artist_edit);
        }

        InvalidateRect(NULL, FALSE);
        return 0;
    }
    bHandled = FALSE;
    return 0;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void GalleryWindow::RecalcLayout()
{
    RECT client;
    GetClientRect(&client);
    int cw = client.right - client.left;

    int cols = (std::max)(1, (cw - THUMB_PADDING) / (THUMB_SIZE + THUMB_PADDING));
    int title_height = 0; // No permanent search bar - thumbnails start at top
    int cell_height = THUMB_SIZE + LABEL_HEIGHT + THUMB_PADDING;
    int separator_height = 28;

    // Separate front, back cover, and artist image indices
    std::vector<int> front_indices, back_indices, artist_indices;
    for (int i = 0; i < (int)m_cells.size(); i++) {
        if (is_artist_image(m_cells[i].entry))
            artist_indices.push_back(i);
        else if (is_back_cover(m_cells[i].entry))
            back_indices.push_back(i);
        else
            front_indices.push_back(i);
    }

    // Layout front covers
    for (int i = 0; i < (int)front_indices.size(); i++) {
        int idx = front_indices[i];
        int col = i % cols;
        int row = i / cols;
        int x = THUMB_PADDING + col * (THUMB_SIZE + THUMB_PADDING);
        int y = title_height + THUMB_PADDING + row * cell_height - m_scroll_y;

        RECT rc;
        rc.left = x;
        rc.top = y;
        rc.right = x + THUMB_SIZE;
        rc.bottom = y + THUMB_SIZE + LABEL_HEIGHT;
        m_cells[idx].rect = rc;
    }

    int front_rows = front_indices.empty() ? 0 : (((int)front_indices.size() - 1) / cols + 1);
    int front_bottom = title_height + THUMB_PADDING + front_rows * cell_height;

    // Layout back covers (with separator)
    int back_top = front_bottom;
    if (!back_indices.empty()) {
        back_top += separator_height;
    }

    for (int i = 0; i < (int)back_indices.size(); i++) {
        int idx = back_indices[i];
        int col = i % cols;
        int row = i / cols;
        int x = THUMB_PADDING + col * (THUMB_SIZE + THUMB_PADDING);
        int y = back_top + THUMB_PADDING + row * cell_height - m_scroll_y;

        RECT rc;
        rc.left = x;
        rc.top = y;
        rc.right = x + THUMB_SIZE;
        rc.bottom = y + THUMB_SIZE + LABEL_HEIGHT;
        m_cells[idx].rect = rc;
    }

    int back_rows = back_indices.empty() ? 0 : (((int)back_indices.size() - 1) / cols + 1);
    int section_bottom = front_bottom;
    if (!back_indices.empty()) {
        section_bottom = back_top + THUMB_PADDING + back_rows * cell_height;
    }

    // Layout artist images (with separator)
    int artist_top = section_bottom;
    if (!artist_indices.empty()) {
        artist_top += separator_height;
    }

    for (int i = 0; i < (int)artist_indices.size(); i++) {
        int idx = artist_indices[i];
        int col = i % cols;
        int row = i / cols;
        int x = THUMB_PADDING + col * (THUMB_SIZE + THUMB_PADDING);
        int y = artist_top + THUMB_PADDING + row * cell_height - m_scroll_y;

        RECT rc;
        rc.left = x;
        rc.top = y;
        rc.right = x + THUMB_SIZE;
        rc.bottom = y + THUMB_SIZE + LABEL_HEIGHT;
        m_cells[idx].rect = rc;
    }

    int artist_rows = artist_indices.empty() ? 0 : (((int)artist_indices.size() - 1) / cols + 1);
    if (!artist_indices.empty()) {
        m_content_height = artist_top + THUMB_PADDING + artist_rows * cell_height;
    } else {
        m_content_height = section_bottom;
    }

    // Position Save button in the status bar area
    int btn_x = cw - 90;
    int btn_y = client.bottom - STATUS_BAR_HEIGHT + 6;
    if (m_save_button && ::IsWindow(m_save_button)) {
        ::MoveWindow(m_save_button, btn_x, btn_y, 80, 28, TRUE);
    }

    // Position floating search panel controls at bottom-right with animation
    if (m_search_anim_t > 0.0f) {
        int ch = client.bottom - client.top;
        int icon_label_w = 24;   // icon-sized labels
        int icon_btn_w = SEARCH_ICON_SIZE;
        int gap = 4;
        int margin = SEARCH_ICON_MARGIN;
        int h = 24;
        int panel_y = ch - STATUS_BAR_HEIGHT - SEARCH_PANEL_HEIGHT - margin
                      + (SEARCH_PANEL_HEIGHT - h) / 2;

        // Ease-out curve: eased = 1 - (1 - t)^2
        float eased = 1.0f - (1.0f - m_search_anim_t) * (1.0f - m_search_anim_t);

        // Panel takes ~85% of window width
        int max_panel_w = cw * 85 / 100;
        // Calculate edit field widths from available space within capped width
        int fixed_w = icon_label_w + gap + icon_label_w + gap + icon_btn_w + gap * 2 + margin;
        int edit_total = max_panel_w - fixed_w;
        int edit_w = (std::max)(60, edit_total / 2);
        int total_content_w = icon_label_w + gap + edit_w + gap + icon_label_w + gap + edit_w + gap + icon_btn_w + margin;

        // Anchor to right edge, slide in from the right
        int slide_offset = (int)((1.0f - eased) * total_content_w);
        int base_x = cw - total_content_w;

        int x = base_x + slide_offset;
        if (m_artist_label && ::IsWindow(m_artist_label))
            ::MoveWindow(m_artist_label, x, panel_y, icon_label_w, h, TRUE);
        x += icon_label_w + gap;

        if (m_artist_edit && ::IsWindow(m_artist_edit))
            ::MoveWindow(m_artist_edit, x, panel_y, edit_w, h, TRUE);
        x += edit_w + gap;

        if (m_album_label && ::IsWindow(m_album_label))
            ::MoveWindow(m_album_label, x, panel_y, icon_label_w, h, TRUE);
        x += icon_label_w + gap;

        if (m_album_edit && ::IsWindow(m_album_edit))
            ::MoveWindow(m_album_edit, x, panel_y, edit_w, h, TRUE);
        x += edit_w + gap;

        if (m_search_button && ::IsWindow(m_search_button))
            ::MoveWindow(m_search_button, x, panel_y, icon_btn_w, h, TRUE);
    }
}

int GalleryWindow::HitTest(POINT pt) const
{
    for (int i = 0; i < (int)m_cells.size(); i++) {
        if (PtInRect(&m_cells[i].rect, pt)) {
            return i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void GalleryWindow::PaintGrid(Gdiplus::Graphics& g)
{
    COLORREF textColor = GetSysColor(COLOR_WINDOWTEXT);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)));
    Gdiplus::SolidBrush grayBrush(Gdiplus::Color(130, 130, 130));
    Gdiplus::Font labelFont(L"Segoe UI", 12, Gdiplus::FontStyleBold);
    Gdiplus::Font smallFont(L"Segoe UI", 12);

    RECT client;
    GetClientRect(&client);
    int cw = client.right - client.left;
    int cols = (std::max)(1, (cw - THUMB_PADDING) / (THUMB_SIZE + THUMB_PADDING));
    int title_height = 0;
    int cell_height = THUMB_SIZE + LABEL_HEIGHT + THUMB_PADDING;
    int separator_height = 28;

    // Count front covers, back covers, and artist images to know where separators go
    int front_count = 0;
    int back_count = 0;
    bool has_backs = false;
    bool has_artists = false;
    for (const auto& cell : m_cells) {
        if (is_artist_image(cell.entry)) {
            has_artists = true;
        } else if (is_back_cover(cell.entry)) {
            has_backs = true;
            back_count++;
        } else {
            front_count++;
        }
    }

    // Draw "Back Covers" separator if there are back covers
    if (has_backs) {
        int front_rows = front_count > 0 ? ((front_count - 1) / cols + 1) : 0;
        int sep_y = title_height + THUMB_PADDING + front_rows * cell_height - m_scroll_y;

        Gdiplus::Pen sepPen(Gdiplus::Color(180, 180, 180));
        g.DrawLine(&sepPen, THUMB_PADDING, sep_y + 4, cw - THUMB_PADDING, sep_y + 4);

        Gdiplus::Font sepFont(L"Segoe UI", 9, Gdiplus::FontStyleBold);
        Gdiplus::RectF sepRect((float)THUMB_PADDING, (float)(sep_y + 8), (float)(cw - THUMB_PADDING * 2), 20.0f);
        g.DrawString(L"Back Covers", -1, &sepFont, sepRect, nullptr, &grayBrush);
    }

    // Draw "Artist Images" separator if there are artist images
    if (has_artists) {
        int front_rows = front_count > 0 ? ((front_count - 1) / cols + 1) : 0;
        int front_bottom = title_height + THUMB_PADDING + front_rows * cell_height;

        int back_top = front_bottom;
        if (has_backs) back_top += separator_height;
        int back_rows = back_count > 0 ? ((back_count - 1) / cols + 1) : 0;
        int section_bottom = has_backs ? (back_top + THUMB_PADDING + back_rows * cell_height) : front_bottom;

        int sep_y = section_bottom - m_scroll_y;

        Gdiplus::Pen sepPen(Gdiplus::Color(180, 180, 180));
        g.DrawLine(&sepPen, THUMB_PADDING, sep_y + 4, cw - THUMB_PADDING, sep_y + 4);

        Gdiplus::Font sepFont(L"Segoe UI", 9, Gdiplus::FontStyleBold);
        Gdiplus::RectF sepRect((float)THUMB_PADDING, (float)(sep_y + 8), (float)(cw - THUMB_PADDING * 2), 20.0f);
        g.DrawString(L"Artist Images", -1, &sepFont, sepRect, nullptr, &grayBrush);
    }

    // Draw all thumbnails and labels
    for (int i = 0; i < (int)m_cells.size(); i++) {
        PaintThumbnail(g, m_cells[i]);

        // Source label
        const auto& entry = m_cells[i].entry;
        int lx = m_cells[i].rect.left;
        int ly = m_cells[i].rect.top + THUMB_SIZE + 2;

        {
            // Strip internal " (Back)" and " (Artist)" suffixes for display
            std::string display_source_str(entry.source.get_ptr());
            size_t back_pos = display_source_str.find(" (Back)");
            if (back_pos != std::string::npos) display_source_str.erase(back_pos);
            size_t artist_pos = display_source_str.find(" (Artist)");
            if (artist_pos != std::string::npos) display_source_str.erase(artist_pos);
            const char* display_source = display_source_str.c_str();

            int len = MultiByteToWideChar(CP_UTF8, 0, display_source, -1, NULL, 0);
            std::wstring wSource(len > 0 ? len - 1 : 0, L'\0');
            if (len > 1) MultiByteToWideChar(CP_UTF8, 0, display_source, -1, &wSource[0], len);

            Gdiplus::RectF labelRect((float)lx, (float)ly, (float)THUMB_SIZE, 34.0f);
            g.DrawString(wSource.c_str(), -1, &labelFont, labelRect, nullptr, &textBrush);
        }

        // Resolution label
        if (entry.width > 0 && entry.height > 0) {
            std::wostringstream ss;
            ss << entry.width << L"x" << entry.height;
            std::wstring res = ss.str();
            Gdiplus::RectF resRect((float)lx, (float)(ly + 34), (float)THUMB_SIZE, 30.0f);
            g.DrawString(res.c_str(), -1, &smallFont, resRect, nullptr, &grayBrush);
        }
    }
}

void GalleryWindow::PaintThumbnail(Gdiplus::Graphics& g, const ThumbnailCell& cell)
{
    // Draw thumbnail image or placeholder
    Gdiplus::Rect thumbRect(cell.rect.left, cell.rect.top, THUMB_SIZE, THUMB_SIZE);

    // Background for thumb area
    Gdiplus::SolidBrush thumbBg(Gdiplus::Color(40, 40, 40));
    g.FillRectangle(&thumbBg, thumbRect);

    if (cell.thumbnail) {
        // Center the thumbnail within the cell
        int tw = cell.thumbnail->GetWidth();
        int th = cell.thumbnail->GetHeight();
        int dx = cell.rect.left + (THUMB_SIZE - tw) / 2;
        int dy = cell.rect.top + (THUMB_SIZE - th) / 2;
        g.DrawImage(cell.thumbnail.get(), dx, dy, tw, th);
    }

    // Selection border
    if (cell.selected) {
        COLORREF hlColor = GetSysColor(COLOR_HIGHLIGHT);
        Gdiplus::Pen selPen(Gdiplus::Color(GetRValue(hlColor), GetGValue(hlColor), GetBValue(hlColor)), 2.0f);
        g.DrawRectangle(&selPen, cell.rect.left - 1, cell.rect.top - 1,
            THUMB_SIZE + 2, THUMB_SIZE + LABEL_HEIGHT + 2);
    }
}

void GalleryWindow::PaintStatusBar(Gdiplus::Graphics& g, const RECT& client)
{
    int cw = client.right - client.left;
    int sy = client.bottom - STATUS_BAR_HEIGHT;

    // Status bar background
    COLORREF bgColor = GetSysColor(COLOR_3DFACE);
    Gdiplus::SolidBrush barBrush(Gdiplus::Color(GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor)));
    g.FillRectangle(&barBrush, 0, sy, cw, STATUS_BAR_HEIGHT);

    // Separator line
    Gdiplus::Pen sepPen(Gdiplus::Color(180, 180, 180));
    g.DrawLine(&sepPen, 0, sy, cw, sy);

    // Status text
    COLORREF textColor = GetSysColor(COLOR_WINDOWTEXT);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)));
    Gdiplus::Font statusFont(L"Segoe UI", 9);
    Gdiplus::RectF textRect(8.0f, (float)(sy + 10), (float)(cw - 100), 20.0f);
    g.DrawString(m_status_text.c_str(), -1, &statusFont, textRect, nullptr, &textBrush);
}

void GalleryWindow::PaintSearchIcon(Gdiplus::Graphics& g)
{
    // Draw the search icon in the bottom-right corner, above the status bar
    RECT client;
    GetClientRect(&client);
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;

    // Hide the floating icon as soon as the panel starts opening
    if (m_search_panel_open) return;

    int icon_x = cw - SEARCH_ICON_SIZE - SEARCH_ICON_MARGIN;
    int icon_y = ch - STATUS_BAR_HEIGHT - SEARCH_ICON_SIZE - SEARCH_ICON_MARGIN;

    float ox = (float)(icon_x + 2);
    float oy = (float)(icon_y + 2);
    float size = 20.0f;

    COLORREF textColor = GetSysColor(COLOR_WINDOWTEXT);
    Gdiplus::Color iconColor(GetRValue(textColor), GetGValue(textColor), GetBValue(textColor));
    Gdiplus::Pen iconPen(iconColor, 2.0f);
    iconPen.SetStartCap(Gdiplus::LineCapRound);
    iconPen.SetEndCap(Gdiplus::LineCapRound);

    float cx = ox + size * 0.42f;
    float cy = oy + size * 0.42f;
    float r = size * 0.30f;

    g.DrawEllipse(&iconPen, cx - r, cy - r, r * 2, r * 2);

    float angle = 0.785f;
    float hx1 = cx + r * cosf(angle);
    float hy1 = cy + r * sinf(angle);
    float hx2 = ox + size * 0.88f;
    float hy2 = oy + size * 0.88f;
    g.DrawLine(&iconPen, hx1, hy1, hx2, hy2);
}

// ---------------------------------------------------------------------------
// Thumbnail Generation
// ---------------------------------------------------------------------------

std::unique_ptr<Gdiplus::Bitmap> GalleryWindow::CreateThumbnail(const artwork_entry& entry)
{
    if (entry.data.get_size() == 0) return nullptr;

    IStream* stream = SHCreateMemStream(entry.data.get_ptr(), (UINT)entry.data.get_size());
    if (!stream) return nullptr;

    Gdiplus::Bitmap srcImage(stream);
    stream->Release();

    if (srcImage.GetLastStatus() != Gdiplus::Ok) return nullptr;

    int srcW = srcImage.GetWidth();
    int srcH = srcImage.GetHeight();
    if (srcW <= 0 || srcH <= 0) return nullptr;

    // Scale to fit within THUMB_SIZE x THUMB_SIZE maintaining aspect ratio
    float scale = (std::min)((float)THUMB_SIZE / srcW, (float)THUMB_SIZE / srcH);
    int dstW = (int)(srcW * scale);
    int dstH = (int)(srcH * scale);
    if (dstW < 1) dstW = 1;
    if (dstH < 1) dstH = 1;

    auto thumb = std::make_unique<Gdiplus::Bitmap>(dstW, dstH, PixelFormat32bppARGB);
    Gdiplus::Graphics tg(thumb.get());
    tg.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    tg.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    tg.DrawImage(&srcImage, 0, 0, dstW, dstH);

    return thumb;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void GalleryWindow::DoSave(bool shift_held)
{
    if (m_selected_index < 0 || m_selected_index >= (int)m_cells.size()) return;
    if (m_album_folder.empty()) return;

    const artwork_entry& entry = m_cells[m_selected_index].entry;

    // Build default save path using configured format (JPEG or PNG)
    const char* base_filename = is_artist_image(entry) ? cfg_ag_artist_image_filename.get_ptr() : is_back_cover(entry) ? cfg_ag_back_cover_filename.get_ptr() : cfg_ag_save_filename.get_ptr();
    bool use_png = (cfg_ag_save_format.get_value() == 1);
    std::string final_filename = replace_extension(std::string(base_filename), use_png ? ".png" : ".jpg");
    std::string save_path = m_album_folder + "\\" + final_filename;

    // Convert to wide string
    int len = MultiByteToWideChar(CP_UTF8, 0, save_path.c_str(), -1, NULL, 0);
    std::wstring wide_path(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) MultiByteToWideChar(CP_UTF8, 0, save_path.c_str(), -1, &wide_path[0], len);

    if (shift_held) {
        // Save As dialog
        WCHAR szFile[MAX_PATH] = {};
        wcsncpy_s(szFile, wide_path.c_str(), _TRUNCATE);

        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        if (use_png) {
            ofn.lpstrFilter = L"PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
            ofn.lpstrDefExt = L"png";
        } else {
            ofn.lpstrFilter = L"JPEG Files (*.jpg)\0*.jpg\0All Files (*.*)\0*.*\0";
            ofn.lpstrDefExt = L"jpg";
        }
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (!GetSaveFileNameW(&ofn)) {
            return; // User cancelled
        }
        wide_path = szFile;
    } else {
        // Check overwrite setting
        int overwrite_mode = cfg_ag_overwrite.get_value();
        DWORD attrs = GetFileAttributesW(wide_path.c_str());
        bool file_exists = (attrs != INVALID_FILE_ATTRIBUTES);

        if (file_exists) {
            if (overwrite_mode == 0) {
                // Ask
                int result = ::MessageBoxW(m_hWnd, L"File already exists. Overwrite?",
                    L"Art Grab", MB_YESNO | MB_ICONQUESTION);
                if (result != IDYES) return;
            } else if (overwrite_mode == 2) {
                // Skip
                m_status_text = L"File already exists";
                InvalidateRect(NULL, FALSE);
                return;
            }
            // overwrite_mode == 1: Always -- fall through
        }
    }

    // Convert image data to JPEG and save
    IStream* stream = SHCreateMemStream(entry.data.get_ptr(), (UINT)entry.data.get_size());
    if (!stream) {
        m_status_text = L"Save failed: could not read image data";
        InvalidateRect(NULL, FALSE);
        return;
    }

    Gdiplus::Image img(stream);
    stream->Release();

    if (img.GetLastStatus() != Gdiplus::Ok) {
        m_status_text = L"Save failed: invalid image data";
        InvalidateRect(NULL, FALSE);
        return;
    }

    CLSID encoderClsid;
    const WCHAR* mime = use_png ? L"image/png" : L"image/jpeg";
    if (GetEncoderClsid(mime, &encoderClsid) < 0) {
        m_status_text = use_png ? L"Save failed: PNG encoder not found" : L"Save failed: JPEG encoder not found";
        InvalidateRect(NULL, FALSE);
        return;
    }

    Gdiplus::Status status;
    if (use_png) {
        status = img.Save(wide_path.c_str(), &encoderClsid, nullptr);
    } else {
        Gdiplus::EncoderParameters params;
        ULONG quality = (ULONG)cfg_ag_jpeg_quality.get_value();
        params.Count = 1;
        params.Parameter[0].Guid = Gdiplus::EncoderQuality;
        params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        params.Parameter[0].NumberOfValues = 1;
        params.Parameter[0].Value = &quality;
        status = img.Save(wide_path.c_str(), &encoderClsid, &params);
    }

    if (status == Gdiplus::Ok) {
        m_status_text = L"Saved to " + wide_path;
        artgrab::invalidate_artwork_cache(m_artist.c_str(), m_album.c_str());
        artgrab::refresh_artwork_panel();
    } else {
        m_status_text = L"Save failed";
    }
    InvalidateRect(NULL, FALSE);
}

void GalleryWindow::DoPreview(int index)
{
    if (index < 0 || index >= (int)m_cells.size()) return;
    auto* preview = new artgrab::PreviewWindow(m_cells[index].entry);
    preview->ShowPopup(m_hWnd);
}

// ---------------------------------------------------------------------------
// JPEG Encoder Helper
// ---------------------------------------------------------------------------

int GalleryWindow::GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;
    UINT size = 0;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    std::vector<BYTE> buffer(size);
    Gdiplus::ImageCodecInfo* pCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    Gdiplus::GetImageEncoders(num, size, pCodecInfo);

    for (UINT i = 0; i < num; i++) {
        if (wcscmp(pCodecInfo[i].MimeType, format) == 0) {
            *pClsid = pCodecInfo[i].Clsid;
            return (int)i;
        }
    }
    return -1;
}

} // namespace artgrab
