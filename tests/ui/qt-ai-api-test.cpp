#include <clipcoach/network/qt-ai-api.hpp>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <cassert>

namespace {

void serveProviderUnavailable(QTcpServer &server)
{
	QObject::connect(&server, &QTcpServer::newConnection, &server, [&server] {
		auto *socket = server.nextPendingConnection();
		QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
			socket->readAll();
			const QByteArray body =
				R"({"error":{"code":"AI_PROVIDER_UNAVAILABLE","message":"Provider unavailable"}})";
			const QByteArray response =
				"HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\n"
				"Connection: close\r\nContent-Length: " +
				QByteArray::number(body.size()) + "\r\n\r\n" + body;
			socket->write(response);
			socket->disconnectFromHost();
		});
	});
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);
	QTcpServer backend;
	assert(backend.listen(QHostAddress::LocalHost));
	serveProviderUnavailable(backend);

	clipcoach::network::QtAiApi api(QList<QUrl>{
		QUrl(QStringLiteral("http://127.0.0.1:%1").arg(backend.serverPort())),
		QUrl(QStringLiteral("http://example.com")),
	});
	clipcoach::AiAssistantRequest request;
	request.requestId = "ai-fallback-regression";
	request.clipId = "clip";
	request.transcript = "transcript";

	bool completed = false;
	api.analyze(std::move(request), "signed-license-token",
		    [&application, &completed](clipcoach::AiAssistantResult result) {
			    assert(!result.success);
			    assert(result.code == "AI_PROVIDER_UNAVAILABLE");
			    assert(result.message == "Provider unavailable");
			    completed = true;
			    application.quit();
		    });
	QTimer::singleShot(3000, &application, &QCoreApplication::quit);
	application.exec();
	assert(completed);
	return 0;
}
