#include "stdafx.h"
#include "artgrab_preferences.h"

// ============================================================================
// GUIDs for cfg_vars
// ============================================================================

// {B1A2C3D4-0001-0001-A1B2-C3D4E5F60718}
static constexpr GUID guid_ag_save_filename = { 0xb1a2c3d4, 0x0001, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };
// {B1A2C3D4-0002-0001-A1B2-C3D4E5F60718}
static constexpr GUID guid_ag_overwrite = { 0xb1a2c3d4, 0x0002, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };
// {B1A2C3D4-0003-0001-A1B2-C3D4E5F60718}
static constexpr GUID guid_ag_jpeg_quality = { 0xb1a2c3d4, 0x0003, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };
// {B1A2C3D4-0004-0001-A1B2-C3D4E5F60718}
static constexpr GUID guid_ag_max_results = { 0xb1a2c3d4, 0x0004, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };
// {B1A2C3D4-0005-0001-A1B2-C3D4E5F60718}
static constexpr GUID guid_ag_http_timeout = { 0xb1a2c3d4, 0x0005, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };
// {B1A2C3D4-0006-0001-A1B2-C3D4E5F60718}
static constexpr GUID guid_ag_retry_count = { 0xb1a2c3d4, 0x0006, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };
// {B1A2C3D4-000D-0001-A1B2-C3D4E5F60718}
static constexpr GUID guid_ag_cache_folder = { 0xb1a2c3d4, 0x000d, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };

// ============================================================================
// cfg_var instantiations
// ============================================================================

cfg_string cfg_ag_save_filename(guid_ag_save_filename, "cover.jpg");
cfg_int cfg_ag_overwrite(guid_ag_overwrite, 0);             // 0=Ask, 1=Always, 2=Skip
cfg_int cfg_ag_jpeg_quality(guid_ag_jpeg_quality, 95);
cfg_int cfg_ag_max_results(guid_ag_max_results, 3);
cfg_int cfg_ag_http_timeout(guid_ag_http_timeout, 10);
cfg_int cfg_ag_retry_count(guid_ag_retry_count, 2);
cfg_string cfg_cache_folder(guid_ag_cache_folder, "");

// ============================================================================
// Preferences page class
// ============================================================================

class artgrab_preferences : public preferences_page_instance {
private:
    HWND m_hwnd;
    preferences_page_callback::ptr m_callback;
    bool m_has_changes;
    fb2k::CCoreDarkModeHooks m_darkMode;

public:
    artgrab_preferences(preferences_page_callback::ptr callback)
        : m_hwnd(nullptr), m_callback(callback), m_has_changes(false), m_darkMode() {}

    void set_hwnd(HWND hwnd) { m_hwnd = hwnd; }
    HWND get_wnd() override { return m_hwnd; }

    t_uint32 get_state() override {
        t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
        if (m_has_changes) state |= preferences_state::changed;
        return state;
    }

    void apply() override {
        apply_settings();
        m_has_changes = false;
        m_callback->on_state_changed();
    }

    void reset() override {
        reset_settings();
        m_has_changes = true;
        m_callback->on_state_changed();
    }

    static INT_PTR CALLBACK ConfigProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    void on_changed() {
        m_has_changes = true;
        m_callback->on_state_changed();
    }

    void apply_settings();
    void reset_settings();
    void update_controls();
};

// ============================================================================
// update_controls: populate all controls from cfg_vars
// ============================================================================

void artgrab_preferences::update_controls()
{
    if (!m_hwnd) return;

    // Save filename
    SetDlgItemTextA(m_hwnd, IDC_SAVE_FILENAME, cfg_ag_save_filename.get_ptr());

    // Overwrite combo box
    HWND hCombo = GetDlgItem(m_hwnd, IDC_OVERWRITE_BEHAVIOR);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"Ask every time");
    SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"Always overwrite");
    SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"Skip if exists");
    SendMessage(hCombo, CB_SETCURSEL, (WPARAM)cfg_ag_overwrite.get_value(), 0);

    // Numeric fields
    SetDlgItemInt(m_hwnd, IDC_JPEG_QUALITY, (UINT)cfg_ag_jpeg_quality.get_value(), FALSE);
    SetDlgItemInt(m_hwnd, IDC_MAX_RESULTS, (UINT)cfg_ag_max_results.get_value(), FALSE);
    SetDlgItemInt(m_hwnd, IDC_REQUEST_TIMEOUT, (UINT)cfg_ag_http_timeout.get_value(), FALSE);
}

// ============================================================================
// apply_settings: read control values back to cfg_vars
// ============================================================================

void artgrab_preferences::apply_settings()
{
    if (!m_hwnd) return;

    // Save filename
    char buf[512];
    GetDlgItemTextA(m_hwnd, IDC_SAVE_FILENAME, buf, sizeof(buf));
    cfg_ag_save_filename = buf;

    // Overwrite combo
    int sel = (int)SendDlgItemMessage(m_hwnd, IDC_OVERWRITE_BEHAVIOR, CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR) cfg_ag_overwrite = sel;

    // Numeric fields
    BOOL ok;
    UINT val;
    val = GetDlgItemInt(m_hwnd, IDC_JPEG_QUALITY, &ok, FALSE);
    if (ok) cfg_ag_jpeg_quality = (int)val;

    val = GetDlgItemInt(m_hwnd, IDC_MAX_RESULTS, &ok, FALSE);
    if (ok) cfg_ag_max_results = (int)val;

    val = GetDlgItemInt(m_hwnd, IDC_REQUEST_TIMEOUT, &ok, FALSE);
    if (ok) cfg_ag_http_timeout = (int)val;
}

// ============================================================================
// reset_settings: restore defaults and update UI
// ============================================================================

void artgrab_preferences::reset_settings()
{
    cfg_ag_save_filename = "cover.jpg";
    cfg_ag_overwrite = 0;
    cfg_ag_jpeg_quality = 95;
    cfg_ag_max_results = 3;
    cfg_ag_http_timeout = 10;
    cfg_ag_retry_count = 2;

    update_controls();
}

// ============================================================================
// Dialog procedure
// ============================================================================

INT_PTR CALLBACK artgrab_preferences::ConfigProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    artgrab_preferences* self = reinterpret_cast<artgrab_preferences*>(GetWindowLongPtr(hwnd, DWLP_USER));

    switch (msg) {
    case WM_INITDIALOG:
    {
        self = reinterpret_cast<artgrab_preferences*>(lp);
        SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)self);
        self->set_hwnd(hwnd);
        self->m_darkMode.AddDialogWithControls(hwnd);
        self->update_controls();
        return TRUE;
    }

    case WM_COMMAND:
        if (!self) break;
        switch (LOWORD(wp)) {
        case IDC_SAVE_FILENAME:
        case IDC_JPEG_QUALITY:
        case IDC_MAX_RESULTS:
        case IDC_REQUEST_TIMEOUT:
            if (HIWORD(wp) == EN_CHANGE) {
                self->on_changed();
            }
            break;

        case IDC_OVERWRITE_BEHAVIOR:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                self->on_changed();
            }
            break;
        }
        break;

    case WM_DESTROY:
        if (self) {
            self->set_hwnd(nullptr);
        }
        break;
    }

    return FALSE;
}

// ============================================================================
// Preferences page factory
// ============================================================================

// {B1A2C3D4-00F0-0001-A1B2-C3D4E5F60718}
static const GUID guid_prefs_page_artgrab =
{ 0xb1a2c3d4, 0x00f0, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };

class artgrab_preferences_page : public preferences_page_v3 {
public:
    const char* get_name() override { return "Artwork Grabber"; }
    GUID get_guid() override { return guid_prefs_page_artgrab; }
    GUID get_parent_guid() override { return preferences_page::guid_tools; }
    preferences_page_instance::ptr instantiate(HWND parent, preferences_page_callback::ptr callback) override;
};

preferences_page_instance::ptr artgrab_preferences_page::instantiate(
    HWND parent, preferences_page_callback::ptr callback)
{
    auto instance = fb2k::service_new<artgrab_preferences>(callback);
    HWND hwnd = CreateDialogParam(
        core_api::get_my_instance(),
        MAKEINTRESOURCE(IDD_PREFERENCES),
        parent,
        artgrab_preferences::ConfigProc,
        reinterpret_cast<LPARAM>(instance.get_ptr()));
    if (!hwnd) throw exception_win32(GetLastError());
    return instance;
}

static preferences_page_factory_t<artgrab_preferences_page> g_prefs_factory;
