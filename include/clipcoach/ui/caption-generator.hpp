#pragma once

#include <clipcoach/core/clip-metadata.hpp>

#include <QString>
#include <QStringList>

#include <functional>

namespace clipcoach::ui {

struct CaptionGenerationResult {
	bool success{false};
	QString caption;
	QString error;
	QString youtubeShortsCaption;
};

inline constexpr int kSocialCaptionMaximumHashtags = 5;
inline constexpr int kSocialCaptionMinimumBodyCharacters = 240;
inline constexpr int kSocialCaptionMaximumBodyCharacters = 420;
inline constexpr int kYouTubeShortsMaximumCharacters = 100;

[[nodiscard]] QString formatSocialCaption(const QString &caption, const QStringList &hashtags = {},
					  const QString &supportingParagraph = {});
[[nodiscard]] QString formatYouTubeShortsCaption(const QString &title, const QString &socialCaption);

struct CaptionGenerationProgress {
	int percentage{0};
	QString status;
	int estimatedSecondsRemaining{0};
};

using CaptionGenerationCompletion = std::function<void(CaptionGenerationResult)>;
using CaptionGenerationProgressCallback = std::function<void(CaptionGenerationProgress)>;
using CaptionGenerator =
	std::function<void(const ClipMetadata &, CaptionGenerationProgressCallback, CaptionGenerationCompletion)>;

} // namespace clipcoach::ui
