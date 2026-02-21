#pragma once
#include "stdafx.h"
#include "artgrab_api.h"
#include <vector>
#include <memory>
#include <string>

namespace artgrab {

class GalleryWindow : public CWindowImpl<GalleryWindow> {
public:
    GalleryWindow(const char* artist, const char* album, const char* file_path);
    ~GalleryWindow();

    DECLARE_WND_CLASS_EX(L"ArtGrabGallery", CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW);

    BEGIN_MSG_MAP(GalleryWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_NCDESTROY, OnNcDestroy)
    END_MSG_MAP()

    void Show(HWND parent);

private:
    // Message handlers
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseWheel(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNcDestroy(UINT, WPARAM, LPARAM, BOOL&);

    // Thumbnail cell data
    struct ThumbnailCell {
        artwork_entry entry;
        std::unique_ptr<Gdiplus::Bitmap> thumbnail; // Scaled preview
        RECT rect;
        bool selected;
        ThumbnailCell() : rect{}, selected(false) {}
    };

    // API loading state
    struct ApiState {
        std::string name;
        bool done;
        bool had_results;
        ApiState(const char* n) : name(n), done(false), had_results(false) {}
    };

    // Layout
    void RecalcLayout();
    int HitTest(POINT pt) const;

    // Drawing
    void PaintGrid(Gdiplus::Graphics& g);
    void PaintThumbnail(Gdiplus::Graphics& g, const ThumbnailCell& cell);
    void PaintStatusBar(Gdiplus::Graphics& g, const RECT& client);

    // Thumbnail generation
    std::unique_ptr<Gdiplus::Bitmap> CreateThumbnail(const artwork_entry& entry);

    // Actions
    void DoSave(bool shift_held);
    void DoPreview(int index);
    void StartSearch();
    void UpdateStatusText();

    // JPEG encoder helper
    static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

    // Search state
    std::string m_artist;
    std::string m_album;
    std::string m_file_path;
    std::string m_album_folder;
    std::shared_ptr<artwork_search> m_search;

    // Results
    std::vector<ThumbnailCell> m_cells;
    std::vector<ApiState> m_api_states;
    int m_selected_index;

    // Status
    std::wstring m_status_text;
    bool m_all_done;

    // Scroll
    int m_scroll_y;
    int m_content_height;

    // Controls
    HWND m_save_button;

    // Layout constants
    static const int THUMB_SIZE = 150;
    static const int THUMB_PADDING = 12;
    static const int LABEL_HEIGHT = 70;
    static const int STATUS_BAR_HEIGHT = 40;
    static const int MIN_WIDTH = 400;
    static const int MIN_HEIGHT = 300;

    static const int ID_SAVE_BUTTON = 2001;

    // Dark mode
    fb2k::CCoreDarkModeHooks m_darkMode;
};

} // namespace artgrab
