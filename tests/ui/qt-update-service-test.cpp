#include "../unit/test-support.hpp"

#include <clipcoach/network/qt-update-checker.hpp>
#include <clipcoach/network/qt-update-downloader.hpp>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

namespace {

void serveOnce(QTcpServer &server, QByteArray body, int status, QByteArray contentType)
{
	QObject::connect(
		&server, &QTcpServer::newConnection, &server,
		[&server, body = std::move(body), status, contentType = std::move(contentType)] {
			auto *socket = server.nextPendingConnection();
			QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, body, status, contentType] {
				socket->readAll();
				const auto response = QByteArray("HTTP/1.1 ") + QByteArray::number(status) +
						      (status == 200 ? " OK\r\n" : " Not Found\r\n") +
						      "Content-Type: " + contentType +
						      "\r\nConnection: close\r\nContent-Length: " +
						      QByteArray::number(body.size()) + "\r\n\r\n" + body;
				socket->write(response);
				socket->disconnectFromHost();
			});
		});
}

QUrl urlFor(const QTcpServer &server, const QString &path = QStringLiteral("/"))
{
	return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);
	QTcpServer unavailableManifest;
	QTcpServer validManifest;
	QTcpServer noPublishedRelease;
	QTcpServer installerServer;
	clipcoach::test::expect(unavailableManifest.listen(QHostAddress::LocalHost) &&
					validManifest.listen(QHostAddress::LocalHost) &&
					noPublishedRelease.listen(QHostAddress::LocalHost) &&
					installerServer.listen(QHostAddress::LocalHost),
				"local update test servers must start");

	const QByteArray installerBytes("verified-clipxtudio-installer");
	const auto installerHash =
		QString::fromLatin1(QCryptographicHash::hash(installerBytes, QCryptographicHash::Sha256).toHex());
	const auto manifest =
		QStringLiteral(
			R"({"version":"0.5.17","download_url":"%1","release_notes_url":"","sha256":"%2","size_bytes":%3})")
			.arg(urlFor(installerServer, QStringLiteral("/setup.exe")).toString(), installerHash)
			.arg(installerBytes.size())
			.toUtf8();
	serveOnce(unavailableManifest, QByteArrayLiteral("{}"), 404, QByteArrayLiteral("application/json"));
	serveOnce(validManifest, manifest, 200, QByteArrayLiteral("application/json"));
	serveOnce(installerServer, installerBytes, 200, QByteArrayLiteral("application/octet-stream"));
	serveOnce(
		noPublishedRelease,
		QByteArrayLiteral(
			R"({"available":false,"status":"coming_soon","message":"No public release is available yet."})"),
		200, QByteArrayLiteral("application/json"));

	clipcoach::network::QtUpdateChecker emptyCatalogChecker(
		urlFor(noPublishedRelease, QStringLiteral("/updates/latest.json")));
	bool emptyCheckCompleted = false;
	clipcoach::network::UpdateCheckResult emptyUpdate;
	emptyCatalogChecker.check(QStringLiteral("0.5.16"),
				  [&application, &emptyCheckCompleted, &emptyUpdate](auto result) {
					  emptyUpdate = std::move(result);
					  emptyCheckCompleted = true;
					  application.quit();
				  });
	QTimer::singleShot(3000, &application, &QCoreApplication::quit);
	application.exec();
	clipcoach::test::expect(emptyCheckCompleted && emptyUpdate.success && !emptyUpdate.updateAvailable &&
					emptyUpdate.errorCode.isEmpty(),
				"an empty published-release catalog must be a valid no-update response");

	clipcoach::network::QtUpdateChecker checker(
		QList<QUrl>{urlFor(unavailableManifest, QStringLiteral("/updates/latest.json")),
			    urlFor(validManifest, QStringLiteral("/updates/latest.json"))});
	clipcoach::test::expect(checker.configured(), "loopback update endpoints must be allowed for local QA");

	bool checkCompleted = false;
	clipcoach::network::UpdateCheckResult update;
	checker.check(QStringLiteral("0.5.16"), [&application, &checkCompleted, &update](auto result) {
		update = std::move(result);
		checkCompleted = true;
		application.quit();
	});
	QTimer::singleShot(3000, &application, &QCoreApplication::quit);
	application.exec();
	clipcoach::test::expect(checkCompleted && update.success && update.updateAvailable &&
					update.latestVersion == QStringLiteral("0.5.17") &&
					update.sha256 == installerHash && update.sizeBytes == installerBytes.size(),
				"checker must fall back, parse version, checksum and installer size");

	QTemporaryDir directory;
	clipcoach::test::expect(directory.isValid(), "temporary update directory must exist");
	const auto destination = directory.filePath(QStringLiteral("ClipXtudio-Setup.exe"));
	clipcoach::network::QtUpdateDownloader downloader;
	bool downloadCompleted = false;
	downloader.download(update, destination, {}, [&application, &downloadCompleted](auto result) {
		downloadCompleted = result.success;
		application.quit();
	});
	QTimer::singleShot(3000, &application, &QCoreApplication::quit);
	application.exec();
	QFile downloaded(destination);
	clipcoach::test::expect(downloadCompleted && downloaded.open(QIODevice::ReadOnly) &&
					downloaded.readAll() == installerBytes,
				"downloader must stream, checksum and atomically save the installer");

	return clipcoach::test::pass("qt-update-service-test");
}
