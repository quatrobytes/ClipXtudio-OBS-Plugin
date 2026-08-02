#include <clipcoach/ui/caption-generator.hpp>

#include <QRegularExpression>
#include <QSet>
#include <QVector>

#include <algorithm>

namespace clipcoach::ui {
namespace {

const QRegularExpression &hashtagExpression()
{
	static const QRegularExpression expression(QStringLiteral(R"((?<!\S)#[\p{L}\p{M}\p{N}_]+)"));
	return expression;
}

QStringList hashtagsIn(const QString &text)
{
	QStringList values;
	auto matches = hashtagExpression().globalMatch(text);
	while (matches.hasNext())
		values.push_back(matches.next().captured(0));
	return values;
}

QString normalizedHashtag(QString value)
{
	value = value.trimmed();
	value.remove(QRegularExpression(QStringLiteral(R"(\s+)")));
	if (!value.isEmpty() && !value.startsWith(QLatin1Char('#')))
		value.prepend(QLatin1Char('#'));
	return hashtagExpression().match(value).captured(0) == value ? value : QString{};
}

int characterCount(const QString &value)
{
	return value.toUcs4().size();
}

QString leftCharacters(const QString &value, int maximum)
{
	if (maximum <= 0)
		return {};
	const auto sourceCodePoints = value.toUcs4();
	if (sourceCodePoints.size() <= maximum)
		return value;
	QVector<char32_t> codePoints;
	codePoints.reserve(maximum);
	for (int index = 0; index < maximum; ++index)
		codePoints.push_back(static_cast<char32_t>(sourceCodePoints[index]));
	return QString::fromUcs4(codePoints.constData(), codePoints.size());
}

QString oneLine(QString value)
{
	value.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
	return value.trimmed();
}

QString boundedSocialBody(QString value)
{
	value = oneLine(std::move(value));
	if (characterCount(value) <= kSocialCaptionMaximumBodyCharacters)
		return value;
	QString clipped = leftCharacters(value, kSocialCaptionMaximumBodyCharacters - 1).trimmed();
	const auto lastSpace = clipped.lastIndexOf(QLatin1Char(' '));
	if (lastSpace >= kSocialCaptionMaximumBodyCharacters * 3 / 4)
		clipped = clipped.left(lastSpace).trimmed();
	return clipped + QChar(0x2026);
}

} // namespace

QString formatSocialCaption(const QString &caption, const QStringList &hashtags, const QString &supportingParagraph)
{
	QStringList candidates = hashtags;
	candidates.append(hashtagsIn(caption));
	QStringList accepted;
	QSet<QString> seen;
	for (const auto &candidate : candidates) {
		const auto normalized = normalizedHashtag(candidate);
		const auto key = normalized.toCaseFolded();
		if (normalized.isEmpty() || seen.contains(key))
			continue;
		seen.insert(key);
		accepted.push_back(normalized);
		if (accepted.size() == kSocialCaptionMaximumHashtags)
			break;
	}

	QString body = caption;
	body.remove(hashtagExpression());
	body = oneLine(body);
	if (characterCount(body) < kSocialCaptionMinimumBodyCharacters) {
		QString supporting = supportingParagraph;
		supporting.remove(hashtagExpression());
		supporting = oneLine(supporting);
		if (!supporting.isEmpty() && !body.contains(supporting, Qt::CaseInsensitive))
			body = body.isEmpty() ? supporting : body + QLatin1Char(' ') + supporting;
	}
	body = boundedSocialBody(body);
	if (accepted.isEmpty())
		return body;
	return body.isEmpty() ? accepted.join(QLatin1Char(' '))
			      : body + QStringLiteral("\n\n") + accepted.join(QLatin1Char(' '));
}

QString formatYouTubeShortsCaption(const QString &title, const QString &socialCaption)
{
	QString cleanTitle = title;
	cleanTitle.remove(hashtagExpression());
	cleanTitle = oneLine(cleanTitle);
	if (cleanTitle.isEmpty()) {
		cleanTitle = socialCaption;
		cleanTitle.remove(hashtagExpression());
		cleanTitle = oneLine(cleanTitle);
	}
	const auto hashtags = hashtagsIn(socialCaption);
	QString result;
	if (!hashtags.isEmpty()) {
		const auto first = hashtags.front();
		const int titleLimit = std::max(0, kYouTubeShortsMaximumCharacters - characterCount(first) - 1);
		result = leftCharacters(cleanTitle, titleLimit).trimmed();
		if (!result.isEmpty())
			result += QLatin1Char(' ');
		result += leftCharacters(first, kYouTubeShortsMaximumCharacters);
		for (int index = 1; index < hashtags.size(); ++index) {
			const auto candidate = QStringLiteral(" ") + hashtags[index];
			if (characterCount(result) + characterCount(candidate) > kYouTubeShortsMaximumCharacters)
				break;
			result += candidate;
		}
	} else {
		result = leftCharacters(cleanTitle, kYouTubeShortsMaximumCharacters).trimmed();
	}
	return leftCharacters(result, kYouTubeShortsMaximumCharacters).trimmed();
}

} // namespace clipcoach::ui
