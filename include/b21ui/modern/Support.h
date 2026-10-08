#pragma once
// A "Support the author" page: link cards that open in the browser, each with a copy button. The
// consuming mod supplies the links in ui21_support_links.inc (see README).

#include <cstdint>
#include <span>
#include <string_view>

namespace b21ui::modern {
    struct SupportLink {
        const char* glyph;  // Font Awesome Brands
        const char* name;
        const char* url;
        std::uint32_t color;  // logo colour, IM_COL32
    };
    std::span<const SupportLink> SupportLinks();

    // Opens `url` in the default browser on a worker thread, so a slow shell never stalls a frame.
    void OpenUrl(const char* url);

    // Fills the current window's content region.
    void SupportPage();
}
