#pragma once

#include "chess/Position.hpp"
#include "platform/Storage.hpp"

#include <3ds.h>
#include <citro2d.h>

#include <cstdint>
#include <string>
#include <vector>

namespace chess3ds::ui {

struct Rect {
    float x{0};
    float y{0};
    float w{0};
    float h{0};

    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct Palette {
    std::uint32_t background;
    std::uint32_t panel;
    std::uint32_t panelAlt;
    std::uint32_t text;
    std::uint32_t muted;
    std::uint32_t accent;
    std::uint32_t danger;
    std::uint32_t lightSquare;
    std::uint32_t darkSquare;
    std::uint32_t selected;
    std::uint32_t lastMove;
};

struct MoveAnimation {
    bool active{false};
    chess::Move move{};
    chess::Piece piece{};
    float progress{0.0f};
};

class Renderer {
public:
    bool initialize();
    void shutdown();

    void beginFrame();
    void beginTop(std::uint32_t clearColor);
    void beginBottom(std::uint32_t clearColor);
    void endFrame();

    void text(const std::string& value, float x, float y, float scale,
              std::uint32_t color, int flags = 0, float wrapWidth = 0.0f);
    void panel(const Rect& rect, std::uint32_t color, float radius = 0.0f);
    void button(const Rect& rect, const std::string& label, bool selected,
                bool disabled, const Palette& palette, const std::string& hint = {});
    void progressBar(const Rect& rect, float progress, std::uint32_t back,
                     std::uint32_t fill);

    void board(const chess::Position& position, const Palette& palette, bool flipped,
               int cursorSquare, int selectedSquare, const std::vector<chess::Move>& legalMoves,
               const chess::Move* lastMove, const MoveAnimation& animation,
               bool showLegalMoves);
    void piece(chess::Piece piece, float x, float y, float size, std::uint8_t alpha = 255);
    void logo(float centerX, float centerY, float size, const Palette& palette);

    static Palette palette(platform::Theme theme);
    static Rect boardRect();
    static int squareFromPoint(float x, float y, bool flipped);
    static Rect squareRect(int square, bool flipped);

private:
    C3D_RenderTarget* top_{nullptr};
    C3D_RenderTarget* bottom_{nullptr};
    C2D_TextBuf textBuffer_{nullptr};

    void outlinedEllipse(float x, float y, float w, float h,
                         std::uint32_t outline, std::uint32_t fill, float border = 1.5f);
    void outlinedRect(float x, float y, float w, float h,
                      std::uint32_t outline, std::uint32_t fill, float border = 1.5f);
};

} // namespace chess3ds::ui
