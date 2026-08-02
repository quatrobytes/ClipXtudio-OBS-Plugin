#include <clipcoach/network/remote-clipper-client.hpp>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <cassert>
#include <iostream>

namespace {

void serve(QTcpServer &server, int &requests, bool &authorizationSeen)
{
	QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
		auto *socket = server.nextPendingConnection();
		QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
			const auto request = socket->readAll();
			++requests;
			authorizationSeen = authorizationSeen ||
					    request.toLower().contains("authorization: bearer signed-token");
			QByteArray body;
			if (request.startsWith("POST /api/plugin/remote/heartbeat")) {
				body = R"({"remote_enabled":true,"poll_interval_seconds":3,"session_id":"77","message":"Remote Clipper active"})";
			} else if (request.startsWith("GET /api/plugin/remote/commands") &&
				   request.contains("denied-device")) {
				body = R"({"error":{"code":"TOKEN_ABILITY_MISSING","message":"The license token cannot perform this action."}})";
				const QByteArray response =
					"HTTP/1.1 403 Forbidden\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
					QByteArray::number(body.size()) + "\r\n\r\n" + body;
				socket->write(response);
				socket->disconnectFromHost();
				return;
			} else if (request.startsWith("GET /api/plugin/remote/commands")) {
				body = R"({"commands":[{"command_uuid":"123e4567-e89b-12d3-a456-426614174000","command_type":"save_vertical","duration_seconds":60,"delay_compensation_seconds":10,"note":"Great moment","requested_by":"editor@example.com","expires_at":"2026-08-01T12:00:00Z"}]})";
			} else {
				body = R"({"status":"completed"})";
			}
			const QByteArray response =
				"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
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
	QTcpServer server;
	if (!server.listen(QHostAddress::LocalHost))
		return 10;
	int requests = 0;
	bool authorizationSeen = false;
	serve(server, requests, authorizationSeen);
	clipcoach::network::RemoteClipperClient client(
		QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
	if (!client.configured())
		return 11;
	int stage = 0;

	clipcoach::remote::RemoteHeartbeatRequest heartbeat;
	heartbeat.deviceActivationId = "device-1";
	heartbeat.pluginVersion = "0.5.64";
	client.heartbeat(heartbeat, "signed-token", [&](auto heartbeatResult) {
		if (!heartbeatResult.succeeded() || !heartbeatResult.value->remoteEnabled) {
			stage = -1;
			application.quit();
			return;
		}
		stage = 1;
		client.commands("device-1", "signed-token", [&](auto commandsResult) {
			if (!commandsResult.succeeded() || commandsResult.value->size() != 1 ||
			    commandsResult.value->front().type != clipcoach::remote::RemoteCommandType::SaveVertical) {
				stage = -2;
				application.quit();
				return;
			}
			stage = 2;
			clipcoach::remote::RemoteCommandResult result;
			result.commandUuid = commandsResult.value->front().uuid;
			result.success = true;
			result.clipId = "clip-1";
			result.fileName = "clip.mp4";
			result.durationSeconds = 60;
			result.orientation = "vertical";
			client.markProcessing(result.commandUuid, "signed-token", [&, result](auto processing) mutable {
				if (!processing.succeeded()) {
					stage = -3;
					application.quit();
					return;
				}
				QTimer::singleShot(0, [&client, &application, &stage, result]() {
					client.reportResult(result, "signed-token", [&](auto report) {
						if (!report.succeeded()) {
							stage = -4;
							application.quit();
							return;
						}
						stage = 3;
						client.commands("denied-device", "signed-token", [&](auto denied) {
							stage = !denied.succeeded() && denied.error.unauthorized &&
										denied.error.code ==
											"TOKEN_ABILITY_MISSING" &&
										denied.error.message ==
											"The license token cannot perform this action."
									? 4
									: -5;
							application.quit();
						});
					});
				});
			});
		});
	});
	QTimer::singleShot(5000, &application, &QCoreApplication::quit);
	application.exec();
	if (stage != 4 || requests != 5 || !authorizationSeen) {
		std::cerr << "remote client stage=" << stage << " requests=" << requests
			  << " authorization=" << authorizationSeen << '\n';
		return 2;
	}
	return 0;
}
