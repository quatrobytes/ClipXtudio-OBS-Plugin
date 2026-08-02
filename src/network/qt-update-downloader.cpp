#include <clipcoach/network/qt-update-downloader.hpp>

#include <QCryptographicHash>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSslConfiguration>
#include <QSslError>

#include <memory>

namespace clipcoach::network {
namespace {

constexpr qint64 kMaximumInstallerBytes = 512LL * 1024LL * 1024LL;

bool allowedDownloadUrl(const QUrl &url)
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

struct DownloadState {
	explicit DownloadState(const QString &path)
		: file(path),
		  hash(QCryptographicHash::Sha256)
	{
	}

	QSaveFile file;
	QCryptographicHash hash;
	qint64 received{0};
	bool writeFailed{false};
	bool sizeExceeded{false};
};

} // namespace

QtUpdateDownloader::QtUpdateDownloader(QNetworkAccessManager *manager)
	: manager_(manager)
{
	if (manager_ == nullptr) {
		ownedManager_ = std::make_unique<QNetworkAccessManager>();
		manager_ = ownedManager_.get();
	}
}

QtUpdateDownloader::~QtUpdateDownloader() = default;

void QtUpdateDownloader::download(const UpdateCheckResult &update, const QString &destinationPath,
				  Progress progress, Completion completion)
{
	static const QRegularExpression checksumPattern(QStringLiteral("^[a-fA-F0-9]{64}$"));
	if (!update.updateAvailable || !allowedDownloadUrl(update.downloadUrl) ||
	    !checksumPattern.match(update.sha256).hasMatch() || update.sizeBytes <= 0 ||
	    update.sizeBytes > kMaximumInstallerBytes || destinationPath.isEmpty()) {
		if (completion)
			completion({false, {}, QStringLiteral("UPDATE_DOWNLOAD_INVALID")});
		return;
	}

	auto state = std::make_shared<DownloadState>(destinationPath);
	if (!state->file.open(QIODevice::WriteOnly)) {
		if (completion)
			completion({false, {}, QStringLiteral("UPDATE_FILE_OPEN_FAILED")});
		return;
	}

	QNetworkRequest request(update.downloadUrl);
	request.setRawHeader("Accept", "application/octet-stream");
	request.setRawHeader("User-Agent", "ClipXtudio-Updater");
	request.setTransferTimeout(120000);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
			     QNetworkRequest::NoLessSafeRedirectPolicy);
	const auto secure =
		update.downloadUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
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
	auto consume = std::make_shared<std::function<void()>>();
	*consume = [reply, state, expected = update.sizeBytes, progress] {
		const auto chunk = reply->readAll();
		if (chunk.isEmpty())
			return;
		state->received += chunk.size();
		if (state->received > expected || state->received > kMaximumInstallerBytes) {
			state->sizeExceeded = true;
			reply->abort();
			return;
		}
		if (state->file.write(chunk) != chunk.size()) {
			state->writeFailed = true;
			reply->abort();
			return;
		}
		state->hash.addData(chunk);
		if (progress)
			progress(state->received, expected);
	};
	QObject::connect(reply, &QNetworkReply::readyRead, reply, [consume] { (*consume)(); });
	QObject::connect(
		reply, &QNetworkReply::finished, reply,
		[reply, state, consume, expected = update.sizeBytes,
		 expectedHash = update.sha256.toLower(), destinationPath,
		 completion = std::move(completion)]() mutable {
			(*consume)();
			QString errorCode;
			if (state->sizeExceeded || state->received != expected)
				errorCode = QStringLiteral("UPDATE_SIZE_MISMATCH");
			else if (state->writeFailed)
				errorCode = QStringLiteral("UPDATE_FILE_WRITE_FAILED");
			else if (reply->error() != QNetworkReply::NoError)
				errorCode = QStringLiteral("UPDATE_DOWNLOAD_NETWORK_ERROR");
			else if (!allowedDownloadUrl(reply->url()))
				errorCode = QStringLiteral("UPDATE_REDIRECT_REJECTED");
			else if (QString::fromLatin1(state->hash.result().toHex()) != expectedHash)
				errorCode = QStringLiteral("UPDATE_CHECKSUM_MISMATCH");
			else if (!state->file.commit())
				errorCode = QStringLiteral("UPDATE_FILE_COMMIT_FAILED");

			if (!errorCode.isEmpty()) {
				state->file.cancelWriting();
				if (completion)
					completion({false, {}, errorCode});
			} else if (completion) {
				completion({true, QFileInfo(destinationPath).absoluteFilePath(), {}});
			}
			reply->deleteLater();
		});
}

} // namespace clipcoach::network
