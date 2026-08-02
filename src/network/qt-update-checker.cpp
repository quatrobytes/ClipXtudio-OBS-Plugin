#include <clipcoach/network/qt-update-checker.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>

#include <array>
#include <optional>
#include <utility>

namespace clipcoach::network {
namespace {

struct ParsedVersion {
	std::array<int, 3> numbers{};
	bool prerelease{false};
};

std::optional<ParsedVersion> parseVersion(const QString &value)
{
	static const QRegularExpression pattern(
		QStringLiteral(R"(^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?$)"));
	const auto match = pattern.match(value.trimmed());
	if (!match.hasMatch())
		return std::nullopt;
	ParsedVersion parsed;
	for (int index = 0; index < 3; ++index) {
		bool valid = false;
		parsed.numbers[index] = match.captured(index + 1).toInt(&valid);
		if (!valid)
			return std::nullopt;
	}
	parsed.prerelease = !match.captured(4).isEmpty();
	return parsed;
}

bool validTransportUrl(const QUrl &url)
{
	if (!url.isValid() || url.host().isEmpty())
		return false;
	if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
		return true;
#ifdef CLIPX_ALLOW_INSECURE_LOCAL_API
	const auto host = url.host().toLower();
	return url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
	       (host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") ||
		host == QStringLiteral("::1"));
#else
	return false;
#endif
}

} // namespace

QtUpdateChecker::QtUpdateChecker(QUrl manifestUrl, QNetworkAccessManager *manager)
	: QtUpdateChecker(QList<QUrl>{std::move(manifestUrl)}, manager)
{
}

QtUpdateChecker::QtUpdateChecker(QList<QUrl> manifestUrls, QNetworkAccessManager *manager)
	: manager_(manager)
{
	for (auto &url : manifestUrls) {
		if (validTransportUrl(url) && !manifestUrls_.contains(url))
			manifestUrls_.push_back(std::move(url));
	}
	configured_ = !manifestUrls_.isEmpty();
	if (manager_ == nullptr) {
		ownedManager_ = std::make_unique<QNetworkAccessManager>();
		manager_ = ownedManager_.get();
	}
}

QtUpdateChecker::~QtUpdateChecker() = default;

void QtUpdateChecker::check(const QString &currentVersion, Completion completion)
{
	if (!configured_) {
		if (completion)
			completion({false, false, {}, {}, {}, {}, 0,
				    QStringLiteral("UPDATE_URL_NOT_CONFIGURED")});
		return;
	}
	if (!QSslSocket::supportsSsl() &&
	    std::any_of(manifestUrls_.cbegin(), manifestUrls_.cend(),
			[](const QUrl &url) {
				return url.scheme().compare(QStringLiteral("https"),
							   Qt::CaseInsensitive) == 0;
			})) {
		if (completion)
			completion({false, false, {}, {}, {}, {}, 0,
				    QStringLiteral("UPDATE_TLS_BACKEND_UNAVAILABLE")});
		return;
	}
	checkCandidate(currentVersion, 0, std::move(completion));
}

void QtUpdateChecker::checkCandidate(const QString &currentVersion, qsizetype candidateIndex,
				     Completion completion)
{
	const auto manifestUrl = manifestUrls_.at(candidateIndex);
	QNetworkRequest request(manifestUrl);
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader("User-Agent", "ClipX-Studio-Update-Checker");
	request.setTransferTimeout(10000);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	const auto secure = manifestUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
	if (secure) {
		auto ssl = request.sslConfiguration();
		ssl.setProtocol(QSsl::TlsV1_2OrLater);
		request.setSslConfiguration(ssl);
	}
	auto *reply = manager_->get(request);
	if (secure) {
		QObject::connect(reply, &QNetworkReply::sslErrors, reply,
				 [reply](const QList<QSslError> &) { reply->abort(); });
	}
	QObject::connect(reply, &QNetworkReply::finished, reply,
			 [this, reply, currentVersion, candidateIndex,
			  completion = std::move(completion)]() mutable {
				 auto result = parseReply(*reply, currentVersion);
				 reply->deleteLater();
				 if (!result.success && candidateIndex + 1 < manifestUrls_.size()) {
					 checkCandidate(currentVersion, candidateIndex + 1,
							std::move(completion));
					 return;
				 }
				 if (completion)
					 completion(std::move(result));
			 });
}

bool QtUpdateChecker::configured() const noexcept
{
	return configured_;
}

bool QtUpdateChecker::isVersionNewer(const QString &candidate, const QString &current)
{
	const auto left = parseVersion(candidate);
	const auto right = parseVersion(current);
	if (!left || !right)
		return false;
	if (left->numbers != right->numbers)
		return left->numbers > right->numbers;
	return right->prerelease && !left->prerelease;
}

UpdateCheckResult QtUpdateChecker::parseReply(QNetworkReply &reply, const QString &currentVersion) const
{
	if (reply.error() != QNetworkReply::NoError)
		return {false, false, {}, {}, {}, {}, 0,
			reply.error() == QNetworkReply::SslHandshakeFailedError
				? QStringLiteral("UPDATE_TLS_ERROR")
				: QStringLiteral("UPDATE_NETWORK_ERROR")};
	if (!validTransportUrl(reply.url()))
		return {false, false, {}, {}, {}, {}, 0,
			QStringLiteral("UPDATE_REDIRECT_REJECTED")};
	const auto payload = reply.readAll();
	if (payload.size() > 64 * 1024)
		return {false, false, {}, {}, {}, {}, 0, QStringLiteral("UPDATE_RESPONSE_TOO_LARGE")};
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(payload, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject())
		return {false, false, {}, {}, {}, {}, 0, QStringLiteral("UPDATE_RESPONSE_INVALID")};
	const auto object = document.object();
	if (object.contains(QStringLiteral("available")) &&
	    !object.value(QStringLiteral("available")).toBool(true))
		return {true, false, {}, {}, {}, {}, 0, {}};
	const auto latestVersion = object.value(QStringLiteral("version")).toString();
	const QUrl downloadUrl(object.value(QStringLiteral("download_url")).toString());
	const QUrl releaseNotesUrl(object.value(QStringLiteral("release_notes_url")).toString());
	const auto sha256 = object.value(QStringLiteral("sha256")).toString().trimmed().toLower();
	const auto sizeBytes = static_cast<qint64>(object.value(QStringLiteral("size_bytes")).toDouble());
	static const QRegularExpression checksumPattern(QStringLiteral("^[a-f0-9]{64}$"));
	if (!parseVersion(latestVersion) || !validTransportUrl(downloadUrl) ||
	    (!releaseNotesUrl.isEmpty() && !validTransportUrl(releaseNotesUrl)) ||
	    !checksumPattern.match(sha256).hasMatch() || sizeBytes <= 0 ||
	    sizeBytes > 512LL * 1024LL * 1024LL)
		return {false, false, {}, {}, {}, {}, 0, QStringLiteral("UPDATE_RESPONSE_INVALID")};
	return {true, isVersionNewer(latestVersion, currentVersion), latestVersion, downloadUrl,
		releaseNotesUrl, sha256, sizeBytes, {}};
}

} // namespace clipcoach::network
