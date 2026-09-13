#ifndef UI_THEME_H
#define UI_THEME_H

#include <M5Dial.h> // For the TFT_* color constants

// Shared accent palette so pages don't drift into inconsistent colors as the
// UI grows. Semantic meaning, not just names:
//   PRIMARY  - "your device state" (heading readout, active selection)
//   TARGET   - navigation target related (arrow, target banner)
//   MARKER   - fixed reference points (lubber line)
//   NORTH    - the compass's north reference specifically (universal red)
//   WARN     - needs attention (no fix, low battery, etc.)
static const uint16_t THEME_ACCENT_PRIMARY = TFT_CYAN;
static const uint16_t THEME_ACCENT_TARGET  = TFT_ORANGE;
static const uint16_t THEME_ACCENT_MARKER  = TFT_GOLD;
static const uint16_t THEME_ACCENT_NORTH   = TFT_RED;
static const uint16_t THEME_WARN           = TFT_ORANGE;

static const uint16_t THEME_TEXT_PRIMARY = TFT_WHITE;
static const uint16_t THEME_TEXT_MUTED   = TFT_LIGHTGREY;
static const uint16_t THEME_TEXT_DIM     = TFT_DARKGREY;

static const uint16_t THEME_BG    = TFT_BLACK;
static const uint16_t THEME_PANEL = TFT_DARKGREY;

#endif // UI_THEME_H
