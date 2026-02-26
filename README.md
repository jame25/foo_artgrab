# foo_artgrab (Artwork Grabber)

A foobar2000 component that lets you browse and download album artwork from multiple online sources. Hover over the artwork panel in [foo_artwork](https://github.com/jame25/foo_artwork) to reveal a download icon, click it to open a gallery of results, preview full-size images, and save your pick as a JPEG to the album folder.

## Features

- Searches only the APIs enabled in foo_artwork preferences (iTunes, Deezer, Last.fm, MusicBrainz/CoverArtArchive, Discogs)
- Up to 3 results per API, displayed progressively as they arrive
- Full-size image preview with fit-to-window / original-size toggle and pan/drag
- Save as JPEG with configurable quality (default 95)
- Shift+Save opens a Save As dialog for custom filename/location
- Configurable overwrite behavior (ask / always / skip)
- Inherits API keys and enabled API settings from foo_artwork automatically
- Download icon hidden for internet streams (radio, YouTube, etc.)

## Requirements

- foobar2000 v1.6+ (64-bit recommended)
- [foo_artwork](https://github.com/jame25/foo_artwork) (provides the download icon overlay on the artwork panel)
- API keys for Last.fm and Discogs configured in foo_artwork preferences

iTunes, Deezer, and MusicBrainz require no API keys.

## Installation

Copy `foo_artgrab.dll` into foobar2000's `components` directory and restart foobar2000.

When foo_artwork is also installed, a download arrow icon appears in the bottom-left corner of the artwork panel on mouse hover. If foo_artwork is not installed, foo_artgrab does nothing (it has no standalone UI entry point).

## Usage

1. Play a track so foo_artwork displays its artwork panel
2. Hover over the artwork panel -- a download arrow icon appears in the bottom-left corner
3. Click the icon to open the **Artwork Grabber** gallery
4. Browse the gallery of results from all enabled APIs
5. Click a thumbnail to select it; **double-click to open a full-size preview**
6. Click **Save** to write the selected image as JPEG to the album folder
7. Hold **Shift** and click **Save** to choose a custom filename/location

## Preferences

Found under **Preferences > Tools > Artwork Grabber**.

| Setting | Default | Description |
|---|---|---|
| Save filename | `cover.jpg` | Default filename written to the album folder |
| Overwrite behavior | Ask every time | Ask / Always overwrite / Skip if exists |
| JPEG quality | 95 | Encoding quality (50-100) |
| Max results per API | 3 | How many images to fetch per API (1-5) |
| Request timeout | 10s | HTTP timeout per request |

API keys and enabled APIs are configured in **Preferences > Tools > Artwork Display** (foo_artwork).

## Building from source

### Prerequisites

- Visual Studio 2022 (v143 toolset)
- Windows 10 SDK

### Build

The project file is at `src/foo_artgrab.vcxproj`. The `src/columns_ui/` directory contains the pre-built foobar2000 SDK and Columns UI SDK libraries. Both Win32 and x64 platforms are supported.

```
msbuild src\foo_artgrab.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild src\foo_artgrab.vcxproj /p:Configuration=Release /p:Platform=Win32
```

Output DLLs are written to `x64\Release\foo_artgrab.dll` and `Win32\Release\foo_artgrab.dll` respectively.

### foo_artwork changes

foo_artgrab requires a small patch to foo_artwork to add the download icon overlay and exported functions. The modified foo_artwork source files are in `foo_artwork-SRC/`. The key changes are:

- `sdk_main.cpp` -- exported functions: `foo_artwork_get_lastfm_key`, `foo_artwork_get_discogs_key`, `foo_artwork_get_discogs_consumer_key`, `foo_artwork_get_discogs_consumer_secret`, `foo_artwork_is_api_enabled`
- `foo_artwork.def` -- lists the exports
- `ui_element.cpp` -- DUI panel: download icon overlay on mouse hover, click opens foo_artgrab gallery
- `artwork_panel_cui.cpp` -- CUI panel: download icon overlay on mouse hover, click opens foo_artgrab gallery

## Architecture

foo_artgrab is a standalone DLL with no compile-time dependency on foo_artwork. The two components communicate at runtime via `GetModuleHandle`/`GetProcAddress`:

- **foo_artwork -> foo_artgrab**: calls `foo_artgrab_open(artist, album, file_path)` when the download icon is clicked
- **foo_artgrab -> foo_artwork**: reads API keys via `foo_artwork_get_lastfm_key()` etc., and checks which APIs are enabled via `foo_artwork_is_api_enabled(api_name)`

If either component is missing, the other continues to function normally.

### Source layout

```
src/
  main.cpp                  Component registration
  artgrab_entry.cpp/h       Exported C function, lifecycle, foo_artwork key/API reading
  artgrab_api.cpp/h         Multi-result API query engine (enabled APIs only, parallel)
  artgrab_gallery.cpp/h     Gallery dialog (ATL CWindowImpl, thumbnail grid)
  artgrab_preview.cpp/h     Full-size image preview popup
  artgrab_preferences.cpp/h Preferences page (save settings only)
  artgrab_utils.cpp/h       String matching, URL encoding, image validation
  async_io_manager.cpp/h    Thread pool, WinHTTP, caching (from foo_artwork)
  webp_decoder.cpp/h        WebP image support (from foo_artwork)
  nlohmann/                 JSON library (header-only)
  columns_ui/               foobar2000 SDK + Columns UI SDK (pre-built libs)
  resource.h                Dialog/control IDs
  foo_artgrab.rc            Dialog resources
  foo_artgrab.def           DLL exports
  foo_artgrab.vcxproj       MSBuild project
```

## License

See individual source files for licensing information.

## Support Development

If you find these components useful, consider supporting development:

| Platform | Payment Methods |
|----------|----------------|
| **[Ko-fi](https://ko-fi.com/Jame25)** | Cards, PayPal |

Your support helps cover development time and enables new features. Thank you! 🙏

---

## 支持开发

如果您觉得这些组件有用，请考虑支持开发：

| 平台 | 支付方式 |
|------|----------|
| **[Ko-fi](https://ko-fi.com/Jame25)** | 银行卡、PayPal |

您的支持有助于支付开发时间并实现新功能。谢谢！🙏

---

**Feature Requests:** Paid feature requests are available for supporters. [Contact me on Discord](https://discord.gg/YB5D5t3x) to discuss.

**功能请求：** 为支持者提供付费功能请求。[请在 Discord 上联系我](https://discord.gg/YB5D5t3x) 进行讨论。



