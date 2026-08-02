#pragma once

#include <clipcoach/network/qt-update-checker.hpp>

#include <functional>
#include <memory>

class QNetworkAccessManager;

namespace clipcoach::network {

struct UpdateDownloadResult {
	bool success{false};
	QString filePath;
	QString errorCode;
};

class QtUpdateDownloader final {
public:
	using Progress = std::function<void(qint64, qint64)>;
	using Completion = std::function<void(UpdateDownloadResult)>;

	explicit QtUpdateDownloader(QNetworkAccessManager *manager = nullptr);
	~QtUpdateDownloader();

	void download(const UpdateCheckResult &update, const QString &destinationPath,
		      Progress progress, Completion completion);

private:
	std::unique_ptr<QNetworkAccessManager> ownedManager_;
	QNetworkAccessManager *manager_{nullptr};
};

} // namespace clipcoach::network
