#include <clipcoach/network/qt-license-api.hpp>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <cassert>

namespace {

void serveOnce(QTcpServer &server, QByteArray responseBody, int status)
{
	QObject::connect(&server, &QTcpServer::newConnection, &server,
			 [&server, responseBody = std::move(responseBody), status] {
				 auto *socket = server.nextPendingConnection();
				 QObject::connect(socket, &QTcpSocket::readyRead, socket,
						  [socket, responseBody, status] {
							  socket->readAll();
							  const auto reason = status == 200 ? "OK" : "Unprocessable Entity";
							  const auto response =
								  QByteArray("HTTP/1.1 ") + QByteArray::number(status) +
								  ' ' + reason + "\r\nContent-Type: application/json\r\n"
									       "Connection: close\r\nContent-Length: " +
								  QByteArray::number(responseBody.size()) +
								  "\r\n\r\n" + responseBody;
							  socket->write(response);
							  socket->disconnectFromHost();
						  });
			 });
}

QUrl serverUrl(const QTcpServer &server)
{
	return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);

	QTcpServer missingLicenseServer;
	QTcpServer activeLicenseServer;
	assert(missingLicenseServer.listen(QHostAddress::LocalHost));
	assert(activeLicenseServer.listen(QHostAddress::LocalHost));

	serveOnce(missingLicenseServer,
		  R"({"error":{"code":"LICENSE_KEY_INVALID","message":"Not in this environment"}})", 422);
	serveOnce(activeLicenseServer,
		  R"({"data":{"license_token":"signed-token","refresh_token":"refresh-token","status":"active"}})",
		  200);

	clipcoach::network::QtLicenseApi api(
		QList<QUrl>{serverUrl(missingLicenseServer), serverUrl(activeLicenseServer)});
	assert(api.configured());

	bool completed = false;
	clipcoach::licensing::ActivationRequest request;
	request.licenseKey = "CC-PRO-TEST";
	request.machineFingerprintHash = "fingerprint";
	request.installId = "install";
	request.requestNonce = "nonce";
	api.activate(std::move(request), [&application, &completed](auto result) {
		assert(result.succeeded());
		assert(result.response->status == "active");
		completed = true;
		application.quit();
	});

	QTimer::singleShot(3000, &application, &QCoreApplication::quit);
	application.exec();
	assert(completed);

	clipcoach::network::QtLicenseApi unsafeRemote(QUrl(QStringLiteral("http://example.com")));
	assert(!unsafeRemote.configured());
	return 0;
}
