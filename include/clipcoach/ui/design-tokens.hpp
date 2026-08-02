#pragma once

#include <QString>

class QMessageBox;

namespace clipcoach::ui::tokens {

inline constexpr int kSpaceXs = 4;
inline constexpr int kSpaceSm = 8;
inline constexpr int kSpaceMd = 12;
inline constexpr int kSpaceLg = 16;
inline constexpr int kSpaceXl = 24;

// Semantic layout metrics shared by every top-level plugin page.
inline constexpr int kPageMargin = kSpaceLg;
inline constexpr int kSectionGap = kSpaceMd;
inline constexpr int kCardPaddingHorizontal = kSpaceLg;
inline constexpr int kCardPaddingVertical = kSpaceMd;
inline constexpr int kPageHeaderIconSize = 44;
inline constexpr int kSummaryCardMinHeight = 68;
inline constexpr int kActionCardMinHeight = 112;

inline constexpr int kRadiusSm = 6;
inline constexpr int kRadiusMd = 9;
inline constexpr int kRadiusLg = 12;

inline constexpr int kControlHeight = 36;
inline constexpr int kFooterControlHeight = 32;
inline constexpr int kLargeControlHeight = 44;
inline constexpr int kIconSize = 16;

inline constexpr const char *kBackground = "#0C1016";
inline constexpr const char *kSurface = "#121821";
inline constexpr const char *kSurfaceRaised = "#171E29";
inline constexpr const char *kSurfaceHover = "#202938";
inline constexpr const char *kBorder = "#283345";
inline constexpr const char *kAccent = "#7C3AED";
inline constexpr const char *kAccentHover = "#8B5CF6";
inline constexpr const char *kAccentPressed = "#6D28D9";
inline constexpr const char *kSuccess = "#22C55E";
inline constexpr const char *kSuccessSurface = "#12351F";
inline constexpr const char *kWarning = "#F59E0B";
inline constexpr const char *kTextPrimary = "#F4F7FB";
inline constexpr const char *kTextSecondary = "#A4AEBD";
inline constexpr const char *kTextMuted = "#6F7B8C";

[[nodiscard]] QString styleSheet();
void configureCompactUpdateDialog(QMessageBox *dialog, int textWidth);
void configureRemoteAuthenticationDialog(QMessageBox *dialog);

} // namespace clipcoach::ui::tokens
