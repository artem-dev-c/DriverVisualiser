#pragma once

#include <QString>

/// ============================================================================
/// AppTheme
/// ============================================================================

namespace AppTheme {

// ── Token struct ──────────────────────────────────────────────────────────────

struct Colors {
    // ── Surfaces ──────────────────────────────────────────────────────────────
    QString bgBase;         ///< Scroll area / outermost background
    QString bgCard;         ///< Driver cards, list rows
    QString bgElevated;     ///< Dashboard, section headers, popups
    QString bgInput;        ///< Search box background
    QString bgInputFocus;   ///< Search box focused
    QString bgHover;        ///< Card/header hover state
    QString bgOverlay;      ///< Popup / notification interior
    QString bgCodeBlock;    ///< Monospace copyable field background
    QString bgIssueRow;     ///< Issue list row background
    QString bgButtonNeutral;///< Neutral button (rescan, clear, log toggle inactive)
    QString bgButtonActive; ///< Active log toggle button
    QString bgButtonReport; ///< Report button background

    // ── Borders ───────────────────────────────────────────────────────────────
    QString borderSubtle;   ///< Healthy card, "no issues" indicator
    QString borderNormal;   ///< Default widget border
    QString borderStrong;   ///< Popup / notification border
    QString borderInput;    ///< Search / filter bar border
    QString borderFocus;    ///< Focused input border (always accent)
    QString borderCodeBlock;///< Monospace field border
    QString borderActive;   ///< Active log toggle border (always accent)
    QString borderButtonNeutral; ///< Neutral button border
    QString borderReport;   ///< Report button border

    // ── Text ──────────────────────────────────────────────────────────────────
    QString textPrimary;    ///< Headings, driver names
    QString textSecondary;  ///< Metadata, sub-labels
    QString textMuted;      ///< "No issues", "Health" sub-label, toggle note
    QString textDisabled;   ///< Disabled / user-disabled items
    QString textSeparator;  ///< · dots between metadata items
    QString textPlaceholder;///< Search bar placeholder
    QString textLabel;      ///< Field labels inside popups (#999 equivalent)
    QString textValue;      ///< Field values inside popups (#eee equivalent)
    QString textCode;       ///< Monospace field text (#ddd equivalent)
    QString textOnAccent;   ///< Text on colored buttons (always white)

    // ── Accent & semantic colors ──────────────────────────────────────────────
    QString accent;         ///< Signature blue
    QString accentDark;     ///< Darker blue (hover on accent buttons)
    QString accentDarker;   ///< Pressed blue
    QString accentBg;       ///< Blue tinted background (badges)
    QString accentBgBorder; ///< Blue tinted border (badges)
    QString accentText;     ///< Blue text on dark bg (active log btn, report btn)

    QString healthy;        ///< Green
    QString warning;        ///< Orange
    QString critical;       ///< Red
    QString caution;        ///< Yellow

    // Softened indicator variants (same as dark — intentionally kept)
    QString warningBorder;  ///< rgba orange at 0.55 for card borders
    QString warningOutline; ///< rgba orange at 0.75 for indicator widgets

    // ── Infrastructure ────────────────────────────────────────────────────────
    QString scrollTrack;    ///< Scrollbar background
    QString scrollHandle;   ///< Scrollbar handle
    QString divider;        ///< Vertical / horizontal divider lines
    QString progressTrack;  ///< Health bar background track
};

// ── API ───────────────────────────────────────────────────────────────────────

/// Detect current Windows theme and populate the internal color set.
/// Must be called before any widget reads colors().
/// Call again when QStyleHints::colorSchemeChanged fires.
void init();

/// Returns the currently active color set.
const Colors& colors();

/// Returns true if the active theme is dark.
bool isDark();

/// Convenience: returns the standard scrollbar stylesheet block.
/// All widgets that host a QScrollArea should use this instead of
/// duplicating the CSS.
QString scrollbarStyleSheet();

} // namespace AppTheme
