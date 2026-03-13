#include "AppTheme.h"
#include <QStyleHints>
#include <QGuiApplication>

namespace AppTheme {

// ── Internal state ────────────────────────────────────────────────────────────

static Colors g_colors;
static bool   g_isDark = true;

// ── Palette builders ──────────────────────────────────────────────────────────

static Colors buildDark()
{
    Colors c;

    // Surfaces
    c.bgBase          = "#1e1e1e";
    c.bgCard          = "#242424";
    c.bgElevated      = "#2b2b2b";
    c.bgInput         = "rgba(43, 43, 43, 0.7)";
    c.bgInputFocus    = "#2b2b2b";
    c.bgHover         = "#353535";
    c.bgOverlay       = "#2b2b2b";
    c.bgCodeBlock     = "#1e1e1e";
    c.bgIssueRow      = "#252525";
    c.bgButtonNeutral = "#3a3a3a";
    c.bgButtonActive  = "#1a3a5c";
    c.bgButtonReport  = "#2d4356";

    // Borders
    c.borderSubtle        = "#2a2a2a";
    c.borderNormal        = "#3d3d3d";
    c.borderStrong        = "#555555";
    c.borderInput         = "#3d3d3d";
    c.borderFocus         = "#5dade2";
    c.borderCodeBlock     = "#3a3a3a";
    c.borderActive        = "#5dade2";
    c.borderButtonNeutral = "#4a4a4a";
    c.borderReport        = "#3d5a6e";

    // Text
    c.textPrimary     = "#ffffff";
    c.textSecondary   = "#909090";
    c.textMuted       = "#555555";
    c.textDisabled    = "#666666";
    c.textSeparator   = "#4a4a4a";
    c.textPlaceholder = "#888888";
    c.textLabel       = "#999999";
    c.textValue       = "#eeeeee";
    c.textCode        = "#dddddd";
    c.textOnAccent    = "#ffffff";

    // Accent & semantic
    c.accent       = "#5dade2";
    c.accentDark   = "#2980b9";
    c.accentDarker = "#1c5a85";
    c.accentBg     = "rgba(93, 173, 226, 0.10)";
    c.accentBgBorder = "rgba(93, 173, 226, 0.30)";
    c.accentText   = "#5dade2";

    c.healthy  = "#27ae60";
    c.warning  = "#f39c12";
    c.critical = "#e74c3c";
    c.caution  = "#f1c40f";

    // Softened variants — intentionally kept from original design
    c.warningBorder  = "rgba(243, 156, 18, 0.55)";
    c.warningOutline = "rgba(243, 156, 18, 0.75)";

    // Infrastructure
    c.scrollTrack   = "#1e1e1e";
    c.scrollHandle  = "#444444";
    c.divider       = "#3d3d3d";
    c.progressTrack = "#1e1e1e";

    return c;
}

static Colors buildLight()
{
    Colors c;

    // Surfaces
    c.bgBase          = "#f0f0f0";
    c.bgCard          = "#ffffff";
    c.bgElevated      = "#f8f8f8";
    c.bgInput         = "#ffffff";
    c.bgInputFocus    = "#ffffff";
    c.bgHover         = "#ebebeb";
    c.bgOverlay       = "#ffffff";
    c.bgCodeBlock     = "#f4f4f4";
    c.bgIssueRow      = "#f5f5f5";
    c.bgButtonNeutral = "#e8e8e8";
    c.bgButtonActive  = "#d0e8f8";
    c.bgButtonReport  = "#daeaf7";

    // Borders
    c.borderSubtle        = "#e0e0e0";
    c.borderNormal        = "#d0d0d0";
    c.borderStrong        = "#b0b0b0";
    c.borderInput         = "#d0d0d0";
    c.borderFocus         = "#2980b9";
    c.borderCodeBlock     = "#d8d8d8";
    c.borderActive        = "#2980b9";
    c.borderButtonNeutral = "#c8c8c8";
    c.borderReport        = "#90bcd8";

    // Text
    c.textPrimary     = "#1a1a1a";
    c.textSecondary   = "#555555";
    c.textMuted       = "#aaaaaa";
    c.textDisabled    = "#999999";
    c.textSeparator   = "#cccccc";
    c.textPlaceholder = "#aaaaaa";
    c.textLabel       = "#777777";
    c.textValue       = "#1a1a1a";
    c.textCode        = "#2a2a2a";
    c.textOnAccent    = "#ffffff";

    // Accent & semantic — same hue family, deeper for light bg legibility
    c.accent       = "#2980b9";
    c.accentDark   = "#1f6391";
    c.accentDarker = "#174e73";
    c.accentBg     = "rgba(41, 128, 185, 0.10)";
    c.accentBgBorder = "rgba(41, 128, 185, 0.30)";
    c.accentText   = "#2980b9";

    c.healthy  = "#1e8449";
    c.warning  = "#d68910";
    c.critical = "#c0392b";
    c.caution  = "#b7950b";

    // Softened variants — mirror the dark design intent on light bg
    c.warningBorder  = "rgba(214, 137, 16, 0.45)";
    c.warningOutline = "rgba(214, 137, 16, 0.65)";

    // Infrastructure
    c.scrollTrack   = "#e8e8e8";
    c.scrollHandle  = "#aaaaaa";
    c.divider       = "#d8d8d8";
    c.progressTrack = "#e0e0e0";

    return c;
}

// ── API ───────────────────────────────────────────────────────────────────────

void init()
{
    Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    g_isDark  = (scheme != Qt::ColorScheme::Light);
    g_colors  = g_isDark ? buildDark() : buildLight();
}

const Colors& colors()
{
    return g_colors;
}

bool isDark()
{
    return g_isDark;
}

QString scrollbarStyleSheet()
{
    const auto& c = g_colors;
    return QString(
        "QScrollBar:vertical {"
        "   background: %1;"
        "   width: 6px;"
        "   border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: %2;"
        "   border-radius: 3px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        "QScrollBar:horizontal {"
        "   background: %1;"
        "   height: 6px;"
        "   border-radius: 3px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "   background: %2;"
        "   border-radius: 3px;"
        "   min-width: 20px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "   width: 0px;"
        "}"
    ).arg(c.scrollTrack, c.scrollHandle);
}

} // namespace AppTheme
