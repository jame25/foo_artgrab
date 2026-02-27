#include "stdafx.h"
#include "artgrab_gallery.h"

// {B1A2C3D4-00A0-0001-A1B2-C3D4E5F60718}
static const GUID guid_contextmenu_artgrab =
{ 0xb1a2c3d4, 0x00a0, 0x0001, { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18 } };

class artgrab_contextmenu : public contextmenu_item_simple {
public:
    GUID get_parent() override {
        return contextmenu_groups::utilities;
    }

    unsigned get_num_items() override { return 1; }

    void get_item_name(unsigned p_index, pfc::string_base& p_out) override {
        if (p_index == 0) p_out = "Grab artwork";
    }

    void get_item_default_path(unsigned p_index, pfc::string_base& p_out) override {
        p_out = "";
    }

    GUID get_item_guid(unsigned p_index) override {
        return guid_contextmenu_artgrab;
    }

    bool get_item_description(unsigned p_index, pfc::string_base& p_out) override {
        if (p_index == 0) {
            p_out = "Browse and download album artwork for the selected track";
            return true;
        }
        return false;
    }

    void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) override {
        if (p_index != 0 || p_data.get_count() == 0) return;

        metadb_handle_ptr track = p_data.get_item(0);

        // Extract artist, album, and file path using titleformat
        pfc::string8 artist, album, path;

        static_api_ptr_t<titleformat_compiler> compiler;
        titleformat_object::ptr tf_artist, tf_album, tf_path;

        compiler->compile_safe_ex(tf_artist, "%artist%");
        compiler->compile_safe_ex(tf_album, "%album%");
        compiler->compile_safe_ex(tf_path, "%path%");

        track->format_title(nullptr, artist, tf_artist, nullptr);
        track->format_title(nullptr, album, tf_album, nullptr);
        track->format_title(nullptr, path, tf_path, nullptr);

        auto* gallery = new artgrab::GalleryWindow(artist.get_ptr(), album.get_ptr(), path.get_ptr());
        gallery->Show(core_api::get_main_window());
    }

    bool context_get_display(unsigned p_index, metadb_handle_list_cref p_data,
                             pfc::string_base& p_out, unsigned& p_displayflags,
                             const GUID& p_caller) override {
        if (p_index != 0) return false;
        if (p_data.get_count() == 0) {
            p_displayflags = FLAG_GRAYED;
        }
        get_item_name(p_index, p_out);
        return true;
    }
};

static contextmenu_item_factory_t<artgrab_contextmenu> g_contextmenu_factory;
