#include "font.hpp"

namespace photorealism {
namespace overlay {

const Glyph* BakedFontView::glyph(unsigned code) const {
    if (font_ == nullptr) {
        return nullptr;
    }
    if (code < kFirstGlyph || code >= kFirstGlyph + kGlyphCount) {
        return nullptr;
    }
    return &font_->glyphs[code - kFirstGlyph];
}

float BakedFontView::line_height() const {
    return font_ != nullptr ? font_->line_height : 0.0f;
}

}
}
