#pragma once

#include <QList>
#include <QUrl>
#include <QString>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace clipcoach::network {

struct UpdateCheckResult {
	bool success{false};
	bool updateAvailable{false};
	QString latestVersion;
	QUrl downloadUrl;
	QUrl releaseNotesUrl;
	QString sha256;
	qint64 sizeBytes{0};
	QString errorCode;
};

class QtUpdateChecker final {
public:
	using Completion = std::function<void(UpdateCheckResult)>;

	explicit QtUpdateChecker(QUrl manifestUrl, QNetworkAccessManager *manager = nullptr);
	explicit QtUpdateChecker(QList<QUrl> manifestUrls, QNetworkAccessManager *manager = nullptr);
	~QtUpdateChecker();

	void check(const QString &currentVersion, Completion completion);
	[[nodiscard]] bool configured() const noexcept;
	[[nodiscard]] static bool isVersionNewer(const QString &candidate, const QString &current);

private:
	void checkCandidate(const QString &currentVersion, qsizetype candidateIndex, Completion completion);
	[[nodiscard]] UpdateCheckResult parseReply(QNetworkReply &reply, const QString &currentVersion) const;

	QList<QUrl> manifestUrls_;
	std::unique_ptr<QNetworkAccessManager> ownedManager_;
	QNetworkAccessManager *manager_{nullptr};
	bool configured_{false};
};

} // namespace clipcoach::network
