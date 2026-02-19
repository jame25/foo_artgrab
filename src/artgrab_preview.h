#pragma once
#include "stdafx.h"
#include "artgrab_api.h"
#include <string>
#include <memory>

namespace artgrab {

class PreviewWindow : public CWindowImpl<PreviewWindow> {
public:
    PreviewWindow(const artwork_entry& entry);
    ~PreviewWindow();

    DECLARE_WND_CLASS_EX(L"ArtGrabPreview", CS_DBLCLKS, COLOR_WINDOW);

    BEGIN_MSG_MAP(PreviewWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_NCDESTROY, OnNcDestroy)
    END_MSG_MAP()

    void ShowPopup(HWND parent);

private:
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnKeyDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNcDestroy(UINT, WPARAM, LPARAM, BOOL&);

    void CalculateImageRect();
    void CenterOnParent(HWND parent);

    // Image
    std::unique_ptr<Gdiplus::Image> m_image;
    std::wstring m_title;
    int m_image_width;
    int m_image_height;

    // Display state
    bool m_fit_to_window;
    RECT m_image_rect;

    // Panning (original size mode)
    int m_pan_offset_x;
    int m_pan_offset_y;
    bool m_is_dragging;
    POINT m_drag_start;
    int m_drag_start_pan_x;
    int m_drag_start_pan_y;
};

} // namespace artgrab
