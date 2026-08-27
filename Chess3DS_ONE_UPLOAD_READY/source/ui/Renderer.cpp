#include "ui/Renderer.hpp"

#include <algorithm>
#include <cmath>

namespace chess3ds::ui {
namespace {

constexpr float BoardX = 40.0f;
constexpr float BoardY = 0.0f;
constexpr float SquareSize = 30.0f;

std::uint32_t withAlpha(std::uint32_t color, std::uint8_t alpha) {
    return (color & 0x00FFFFFFu) | (static_cast<std::uint32_t>(alpha) << 24u);
}

float ease(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

} // namespace

bool Renderer::initialize() {
    top_ = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom_ = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textBuffer_ = C2D_TextBufNew(2048);
    return top_ && bottom_ && textBuffer_;
}

void Renderer::shutdown() {
    if (textBuffer_) C2D_TextBufDelete(textBuffer_);
    textBuffer_ = nullptr;
    top_ = nullptr;
    bottom_ = nullptr;
}

void Renderer::beginFrame() {
    C2D_TextBufClear(textBuffer_);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void Renderer::beginTop(std::uint32_t clearColor) {
    C2D_TargetClear(top_, clearColor);
    C2D_SceneBegin(top_);
}

void Renderer::beginBottom(std::uint32_t clearColor) {
    C2D_TargetClear(bottom_, clearColor);
    C2D_SceneBegin(bottom_);
}

void Renderer::endFrame() {
    C3D_FrameEnd(0);
}

void Renderer::text(const std::string& value, float x, float y, float scale,
                    std::uint32_t color, int flags, float wrapWidth) {
    C2D_Text parsed;
    C2D_TextParse(&parsed, textBuffer_, value.c_str());
    C2D_TextOptimize(&parsed);
    const std::uint32_t drawFlags = static_cast<std::uint32_t>(flags) | C2D_WithColor
        | (wrapWidth > 0.0f ? C2D_WordWrap : 0);
    if (wrapWidth > 0.0f)
        C2D_DrawText(&parsed, drawFlags, x, y, 0.8f, scale, scale, color, wrapWidth);
    else
        C2D_DrawText(&parsed, drawFlags, x, y, 0.8f, scale, scale, color);
}

void Renderer::panel(const Rect& rect, std::uint32_t color, float radius) {
    if (radius <= 0.0f) {
        C2D_DrawRectSolid(rect.x, rect.y, 0.2f, rect.w, rect.h, color);
        return;
    }
    const float r = std::min({radius, rect.w * 0.5f, rect.h * 0.5f});
    C2D_DrawRectSolid(rect.x + r, rect.y, 0.2f, rect.w - 2.0f * r, rect.h, color);
    C2D_DrawRectSolid(rect.x, rect.y + r, 0.2f, rect.w, rect.h - 2.0f * r, color);
    C2D_DrawCircleSolid(rect.x + r, rect.y + r, 0.2f, r, color);
    C2D_DrawCircleSolid(rect.x + rect.w - r, rect.y + r, 0.2f, r, color);
    C2D_DrawCircleSolid(rect.x + r, rect.y + rect.h - r, 0.2f, r, color);
    C2D_DrawCircleSolid(rect.x + rect.w - r, rect.y + rect.h - r, 0.2f, r, color);
}

void Renderer::button(const Rect& rect, const std::string& label, bool selected,
                      bool disabled, const Palette& colors, const std::string& hint) {
    const std::uint32_t fill = selected ? colors.accent : colors.panel;
    panel(rect, disabled ? withAlpha(colors.panel, 150) : fill, 5.0f);
    if (selected)
        C2D_DrawRectSolid(rect.x, rect.y, 0.5f, 4.0f, rect.h, colors.text);
    const std::uint32_t textColor = disabled ? colors.muted : (selected ? colors.background : colors.text);
    text(label, rect.x + 12.0f, rect.y + 6.0f, 0.48f, textColor);
    if (!hint.empty())
        text(hint, rect.x + rect.w - 10.0f, rect.y + 7.0f, 0.42f, textColor, C2D_AlignRight);
}

void Renderer::progressBar(const Rect& rect, float progress, std::uint32_t back,
                           std::uint32_t fill) {
    panel(rect, back, rect.h * 0.5f);
    progress = std::clamp(progress, 0.0f, 1.0f);
    if (progress > 0.0f)
        panel(Rect{rect.x, rect.y, rect.w * progress, rect.h}, fill, rect.h * 0.5f);
}

Palette Renderer::palette(platform::Theme theme) {
    switch (theme) {
        case platform::Theme::Midnight:
            return {C2D_Color32(11, 17, 26, 255), C2D_Color32(26, 37, 52, 255),
                    C2D_Color32(35, 49, 68, 255), C2D_Color32(239, 246, 255, 255),
                    C2D_Color32(146, 163, 184, 255), C2D_Color32(75, 195, 232, 255),
                    C2D_Color32(239, 92, 92, 255), C2D_Color32(151, 171, 191, 255),
                    C2D_Color32(55, 78, 104, 255), C2D_Color32(75, 195, 232, 150),
                    C2D_Color32(234, 192, 84, 130)};
        case platform::Theme::Forest:
            return {C2D_Color32(18, 31, 24, 255), C2D_Color32(31, 53, 40, 255),
                    C2D_Color32(42, 68, 51, 255), C2D_Color32(242, 239, 218, 255),
                    C2D_Color32(166, 180, 157, 255), C2D_Color32(223, 184, 76, 255),
                    C2D_Color32(222, 89, 76, 255), C2D_Color32(207, 218, 187, 255),
                    C2D_Color32(74, 119, 83, 255), C2D_Color32(239, 196, 80, 150),
                    C2D_Color32(118, 196, 147, 135)};
        case platform::Theme::Rosewood:
            return {C2D_Color32(35, 20, 23, 255), C2D_Color32(62, 34, 39, 255),
                    C2D_Color32(79, 43, 48, 255), C2D_Color32(250, 238, 221, 255),
                    C2D_Color32(190, 159, 148, 255), C2D_Color32(231, 180, 90, 255),
                    C2D_Color32(235, 93, 93, 255), C2D_Color32(227, 200, 177, 255),
                    C2D_Color32(139, 75, 75, 255), C2D_Color32(244, 190, 85, 150),
                    C2D_Color32(113, 185, 137, 135)};
        case platform::Theme::Classic:
        case platform::Theme::Count:
            return {C2D_Color32(15, 22, 29, 255), C2D_Color32(28, 39, 49, 255),
                    C2D_Color32(38, 51, 62, 255), C2D_Color32(247, 241, 226, 255),
                    C2D_Color32(157, 170, 177, 255), C2D_Color32(105, 190, 116, 255),
                    C2D_Color32(231, 82, 82, 255), C2D_Color32(235, 220, 193, 255),
                    C2D_Color32(111, 145, 94, 255), C2D_Color32(247, 206, 75, 150),
                    C2D_Color32(104, 190, 116, 130)};
    }
    return palette(platform::Theme::Classic);
}

Rect Renderer::boardRect() { return {BoardX, BoardY, SquareSize * 8.0f, SquareSize * 8.0f}; }

Rect Renderer::squareRect(int square, bool flipped) {
    if (!chess::validSquare(square)) return {};
    int file = chess::fileOf(square);
    int rank = chess::rankOf(square);
    const int screenFile = flipped ? 7 - file : file;
    const int screenRank = flipped ? rank : 7 - rank;
    return {BoardX + screenFile * SquareSize, BoardY + screenRank * SquareSize,
            SquareSize, SquareSize};
}

int Renderer::squareFromPoint(float x, float y, bool flipped) {
    if (!boardRect().contains(x, y)) return chess::NoSquare;
    const int screenFile = static_cast<int>((x - BoardX) / SquareSize);
    const int screenRank = static_cast<int>((y - BoardY) / SquareSize);
    const int file = flipped ? 7 - screenFile : screenFile;
    const int rank = flipped ? screenRank : 7 - screenRank;
    return rank * 8 + file;
}

void Renderer::board(const chess::Position& position, const Palette& colors, bool flipped,
                     int cursorSquare, int selectedSquare, const std::vector<chess::Move>& legalMoves,
                     const chess::Move* lastMove, const MoveAnimation& animation,
                     bool showLegalMoves) {
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, colors.background);
    for (int square = 0; square < 64; ++square) {
        const Rect rect = squareRect(square, flipped);
        const bool light = ((chess::fileOf(square) + chess::rankOf(square)) & 1) != 0;
        C2D_DrawRectSolid(rect.x, rect.y, 0.1f, rect.w, rect.h,
                          light ? colors.lightSquare : colors.darkSquare);
    }

    if (lastMove) {
        for (int square : {static_cast<int>(lastMove->from), static_cast<int>(lastMove->to)}) {
            const Rect rect = squareRect(square, flipped);
            C2D_DrawRectSolid(rect.x, rect.y, 0.2f, rect.w, rect.h, colors.lastMove);
        }
    }
    if (chess::validSquare(selectedSquare)) {
        const Rect rect = squareRect(selectedSquare, flipped);
        C2D_DrawRectSolid(rect.x, rect.y, 0.3f, rect.w, rect.h, colors.selected);
    }

    const int checkedKing = position.inCheck(position.sideToMove())
        ? position.kingSquare(position.sideToMove()) : chess::NoSquare;
    if (checkedKing != chess::NoSquare) {
        const Rect rect = squareRect(checkedKing, flipped);
        C2D_DrawCircleSolid(rect.x + 15.0f, rect.y + 15.0f, 0.35f, 13.0f,
                            withAlpha(colors.danger, 165));
    }

    if (showLegalMoves && chess::validSquare(selectedSquare)) {
        for (const chess::Move& move : legalMoves) {
            const Rect rect = squareRect(move.to, flipped);
            if (move.isCapture()) {
                const std::uint32_t dot = withAlpha(colors.accent, 190);
                C2D_DrawCircleSolid(rect.x + 15.0f, rect.y + 15.0f, 0.4f, 13.0f, dot);
                C2D_DrawCircleSolid(rect.x + 15.0f, rect.y + 15.0f, 0.41f, 9.0f,
                                    ((chess::fileOf(move.to) + chess::rankOf(move.to)) & 1)
                                        ? colors.lightSquare : colors.darkSquare);
            } else {
                C2D_DrawCircleSolid(rect.x + 15.0f, rect.y + 15.0f, 0.4f, 4.2f,
                                    withAlpha(colors.accent, 210));
            }
        }
    }

    for (int square = 0; square < 64; ++square) {
        const chess::Piece value = position.pieceAt(square);
        if (!value) continue;
        if (animation.active && square == animation.move.to) continue;
        const Rect rect = squareRect(square, flipped);
        piece(value, rect.x, rect.y, rect.w);
    }

    if (animation.active) {
        const Rect from = squareRect(animation.move.from, flipped);
        const Rect to = squareRect(animation.move.to, flipped);
        const float t = ease(animation.progress);
        piece(animation.piece, from.x + (to.x - from.x) * t,
              from.y + (to.y - from.y) * t, SquareSize);
    }

    if (chess::validSquare(cursorSquare)) {
        const Rect rect = squareRect(cursorSquare, flipped);
        const std::uint32_t cursor = withAlpha(colors.text, 230);
        C2D_DrawRectSolid(rect.x, rect.y, 0.8f, rect.w, 1.5f, cursor);
        C2D_DrawRectSolid(rect.x, rect.y + rect.h - 1.5f, 0.8f, rect.w, 1.5f, cursor);
        C2D_DrawRectSolid(rect.x, rect.y, 0.8f, 1.5f, rect.h, cursor);
        C2D_DrawRectSolid(rect.x + rect.w - 1.5f, rect.y, 0.8f, 1.5f, rect.h, cursor);
    }

    for (int i = 0; i < 8; ++i) {
        const char fileLabel[2] = {static_cast<char>('a' + (flipped ? 7 - i : i)), '\0'};
        const char rankLabel[2] = {static_cast<char>('1' + (flipped ? i : 7 - i)), '\0'};
        text(fileLabel, BoardX + i * SquareSize + 2.0f, 226.0f, 0.30f, colors.muted);
        text(rankLabel, 29.0f, i * SquareSize + 8.0f, 0.34f, colors.muted, C2D_AlignCenter);
    }
}

void Renderer::outlinedEllipse(float x, float y, float w, float h,
                               std::uint32_t outline, std::uint32_t fill, float border) {
    C2D_DrawEllipseSolid(x, y, 0.61f, w, h, outline);
    C2D_DrawEllipseSolid(x + border, y + border, 0.62f,
                         std::max(0.0f, w - border * 2.0f),
                         std::max(0.0f, h - border * 2.0f), fill);
}

void Renderer::outlinedRect(float x, float y, float w, float h,
                            std::uint32_t outline, std::uint32_t fill, float border) {
    C2D_DrawRectSolid(x, y, 0.61f, w, h, outline);
    C2D_DrawRectSolid(x + border, y + border, 0.62f,
                      std::max(0.0f, w - border * 2.0f),
                      std::max(0.0f, h - border * 2.0f), fill);
}

void Renderer::piece(chess::Piece value, float x, float y, float size, std::uint8_t alpha) {
    if (!value) return;
    const float s = size / 30.0f;
    auto X = [&](float valueX) { return x + valueX * s; };
    auto Y = [&](float valueY) { return y + valueY * s; };
    const std::uint32_t outline = withAlpha(value.color == chess::Color::White
        ? C2D_Color32(33, 38, 43, 255) : C2D_Color32(231, 220, 196, 255), alpha);
    const std::uint32_t fill = withAlpha(value.color == chess::Color::White
        ? C2D_Color32(247, 239, 219, 255) : C2D_Color32(42, 47, 54, 255), alpha);
    const float b = 1.25f * s;

    auto base = [&]() {
        outlinedEllipse(X(5), Y(24), 20 * s, 4 * s, outline, fill, b);
        outlinedRect(X(7), Y(21), 16 * s, 4 * s, outline, fill, b);
    };
    auto body = [&]() {
        C2D_DrawTriangle(X(9), Y(21), outline, X(21), Y(21), outline,
                         X(18), Y(11), outline, 0.61f);
        C2D_DrawTriangle(X(10.5f), Y(20.5f), fill, X(19.5f), Y(20.5f), fill,
                         X(17), Y(12.5f), fill, 0.62f);
    };

    switch (value.type) {
        case chess::PieceType::Pawn:
            base();
            body();
            outlinedEllipse(X(10), Y(5), 10 * s, 10 * s, outline, fill, b);
            break;
        case chess::PieceType::Rook:
            base();
            outlinedRect(X(9), Y(10), 12 * s, 13 * s, outline, fill, b);
            outlinedRect(X(7), Y(7), 16 * s, 5 * s, outline, fill, b);
            for (float towerX : {7.0f, 13.0f, 19.0f})
                outlinedRect(X(towerX), Y(4), 4 * s, 5 * s, outline, fill, b * 0.7f);
            break;
        case chess::PieceType::Knight:
            base();
            C2D_DrawTriangle(X(8), Y(23), outline, X(21), Y(23), outline,
                             X(14), Y(7), outline, 0.61f);
            C2D_DrawTriangle(X(9.5f), Y(21.5f), fill, X(19.5f), Y(21.5f), fill,
                             X(14.2f), Y(8.8f), fill, 0.62f);
            C2D_DrawTriangle(X(12), Y(9), outline, X(22), Y(14), outline,
                             X(12), Y(17), outline, 0.63f);
            C2D_DrawTriangle(X(13), Y(10.5f), fill, X(20), Y(14), fill,
                             X(13), Y(15.4f), fill, 0.64f);
            C2D_DrawTriangle(X(12), Y(10), outline, X(11), Y(4), outline,
                             X(16), Y(8), outline, 0.63f);
            C2D_DrawCircleSolid(X(15.4f), Y(10.7f), 0.7f, 1.1f * s, outline);
            break;
        case chess::PieceType::Bishop:
            base();
            body();
            outlinedEllipse(X(9), Y(4), 12 * s, 12 * s, outline, fill, b);
            C2D_DrawLine(X(12), Y(13), outline, X(18), Y(6), outline, 1.8f * s, 0.75f);
            break;
        case chess::PieceType::Queen:
            base();
            body();
            C2D_DrawTriangle(X(8), Y(14), outline, X(22), Y(14), outline,
                             X(10), Y(6), outline, 0.61f);
            C2D_DrawTriangle(X(9.5f), Y(13), fill, X(20.5f), Y(13), fill,
                             X(11), Y(7.5f), fill, 0.62f);
            for (float crownX : {9.0f, 15.0f, 21.0f})
                outlinedEllipse(X(crownX - 2.0f), Y(crownX == 15.0f ? 3.0f : 5.0f),
                                4 * s, 4 * s, outline, fill, b * 0.65f);
            break;
        case chess::PieceType::King:
            base();
            body();
            outlinedEllipse(X(10), Y(8), 10 * s, 8 * s, outline, fill, b);
            outlinedRect(X(13.5f), Y(2), 3 * s, 8 * s, outline, fill, b * 0.55f);
            outlinedRect(X(11), Y(4), 8 * s, 3 * s, outline, fill, b * 0.55f);
            break;
        case chess::PieceType::None:
            break;
    }
}

void Renderer::logo(float centerX, float centerY, float size, const Palette& colors) {
    const Rect tile{centerX - size * 0.5f, centerY - size * 0.5f, size, size};
    panel(tile, colors.panelAlt, size * 0.18f);
    chess::Piece knight{chess::PieceType::Knight, chess::Color::White};
    piece(knight, tile.x + size * 0.12f, tile.y + size * 0.12f, size * 0.76f);
    C2D_DrawRectSolid(tile.x + size * 0.08f, tile.y + size * 0.08f, 0.7f,
                      size * 0.12f, size * 0.12f, colors.accent);
}

} // namespace chess3ds::ui
