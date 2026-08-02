#include <clipcoach/network/remote-clipper-client.hpp>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>

namespace clipcoach::network {
namespace {

constexpr int kNetworkTimeoutMs = 10'000;

bool safeBaseUrl(const QUrl &url)
{
	if (!url.isValid() || url.host().isEmpty()) return false;
	if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0) return true;
#ifdef CLIPX_ALLOW_INSECURE_LOCAL_API
	return url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
	       (url.host() == QStringLiteral("127.0.0.1") ||
		url.host().compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0);
#else
	return false;
#endif
}

QNetworkRequest makeRequest(const QUrl &url, const std::string &token)
{
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader("Authorization", QByteArray("Bearer ") + QByteArray::fromStdString(token));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setTransferTimeout(kNetworkTimeoutMs);
	return request;
}

remote::RemoteClientError replyError(QNetworkReply &reply, const QJsonObject &body)
{
	const auto status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const auto nestedError = body.value(QStringLiteral("error")).toObject();
	const auto code = body.value(QStringLiteral("code")).toString(
		body.value(QStringLiteral("error_code")).toString(
			nestedError.value(QStringLiteral("code")).toString()));
	auto message = body.value(QStringLiteral("message")).toString(
		nestedError.value(QStringLiteral("message")).toString());
	if (message.isEmpty()) message = reply.errorString();
	return {status, code.toStdString(), message.toStdString(),
		status == 0 || status == 408 || status == 429 || status >= 500,
		status == 401 || status == 403};
}

QJsonObject parseObject(QNetworkReply &reply, bool *valid)
{
	QJsonParseError error;
	const auto document = QJsonDocument::fromJson(reply.readAll(), &error);
	*valid = error.error == QJsonParseError::NoError && document.isObject();
	return *valid ? document.object() : QJsonObject{};
}

remote::RemoteCommand parseCommand(const QJsonObject &object)
{
	remote::RemoteCommand command;
	command.uuid = object.value(QStringLiteral("command_uuid")).toString().toStdString();
	command.type = remote::remoteCommandTypeFromString(
		object.value(QStringLiteral("command_type")).toString().toStdString());
	command.durationSeconds = object.value(QStringLiteral("duration_seconds")).toInt();
	command.delayCompensationSeconds = object.value(QStringLiteral("delay_compensation_seconds")).toInt();
	command.note = object.value(QStringLiteral("note")).toString().toStdString();
	command.requestedBy = object.value(QStringLiteral("requested_by")).toString().toStdString();
	const auto expiry = QDateTime::fromString(object.value(QStringLiteral("expires_at")).toString(), Qt::ISODate);
	if (expiry.isValid())
		command.expiresAt = std::chrono::system_clock::time_point(std::chrono::milliseconds(expiry.toMSecsSinceEpoch()));
	return command;
}

} // namespace

RemoteClipperClient::RemoteClipperClient(QUrl baseUrl, QNetworkAccessManager *manager)
	: baseUrl_(std::move(baseUrl)), manager_(manager)
{
	if (manager_ == nullptr) {
		ownedManager_ = std::make_unique<QNetworkAccessManager>();
		manager_ = ownedManager_.get();
	}
}

RemoteClipperClient::~RemoteClipperClient() = default;
bool RemoteClipperClient::configured() const noexcept { return manager_ != nullptr && safeBaseUrl(baseUrl_); }

QUrl RemoteClipperClient::endpoint(const QString &path) const
{
	auto url = baseUrl_;
	url.setPath(path);
	url.setQuery(QString{});
	url.setFragment({});
	return url;
}

void RemoteClipperClient::heartbeat(const remote::RemoteHeartbeatRequest &value, const std::string &token,
				    HeartbeatCompletion completion)
{
	if (!configured() || token.empty()) {
		completion({{}, {0, "REMOTE_API_NOT_CONFIGURED", "Remote Clipper API or token is unavailable", false,
				 token.empty()}});
		return;
	}
	const QJsonObject body{
		{QStringLiteral("device_activation_id"), QString::fromStdString(value.deviceActivationId)},
		{QStringLiteral("plugin_version"), QString::fromStdString(value.pluginVersion)},
		{QStringLiteral("obs_version"), QString::fromStdString(value.obsVersion)},
		{QStringLiteral("replay_buffer_status"), QString::fromStdString(value.replayBufferStatus)},
		{QStringLiteral("vertical_canvas_status"), QString::fromStdString(value.verticalCanvasStatus)},
		{QStringLiteral("current_scene"), QString::fromStdString(value.currentScene)},
		{QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
	};
	auto *reply = manager_->post(makeRequest(endpoint(QStringLiteral("/api/plugin/remote/heartbeat")), token),
				     QJsonDocument(body).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply, completion = std::move(completion)]() mutable {
		bool valid = false;
		const auto body = parseObject(*reply, &valid);
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300 || !valid) {
			auto error = replyError(*reply, body);
			if (!valid && error.code.empty()) error.code = "INVALID_RESPONSE";
			completion({{}, std::move(error)});
			reply->deleteLater();
			return;
		}
		remote::RemoteHeartbeatResponse response;
		response.remoteEnabled = body.value(QStringLiteral("remote_enabled")).toBool();
		response.pollIntervalSeconds = std::clamp(body.value(QStringLiteral("poll_interval_seconds")).toInt(3), 2, 60);
		response.sessionId = body.value(QStringLiteral("session_id")).toVariant().toString().toStdString();
		response.message = body.value(QStringLiteral("message")).toString().toStdString();
		completion({response, {}});
		reply->deleteLater();
	});
}

void RemoteClipperClient::commands(const std::string &deviceId, const std::string &token,
				   CommandsCompletion completion)
{
	if (!configured() || token.empty() || deviceId.empty()) {
		completion({{}, {0, "REMOTE_AUTH_UNAVAILABLE", "Remote device identity or token is unavailable", false,
				 true}});
		return;
	}
	auto url = endpoint(QStringLiteral("/api/plugin/remote/commands"));
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("device_activation_id"), QString::fromStdString(deviceId));
	url.setQuery(query);
	auto *reply = manager_->get(makeRequest(url, token));
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply, completion = std::move(completion)]() mutable {
		bool valid = false;
		const auto body = parseObject(*reply, &valid);
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300 || !valid ||
		    !body.value(QStringLiteral("commands")).isArray()) {
			auto error = replyError(*reply, body);
			if (error.code.empty()) error.code = "INVALID_RESPONSE";
			completion({{}, std::move(error)});
			reply->deleteLater();
			return;
		}
		std::vector<remote::RemoteCommand> commands;
		for (const auto &entry : body.value(QStringLiteral("commands")).toArray())
			if (entry.isObject()) commands.push_back(parseCommand(entry.toObject()));
		completion({std::move(commands), {}});
		reply->deleteLater();
	});
}

void RemoteClipperClient::reportResult(const remote::RemoteCommandResult &value, const std::string &token,
				       ResultCompletion completion)
{
	if (!configured() || token.empty() || !remote::isValidCommandUuid(value.commandUuid)) {
		completion({{}, {0, "INVALID_RESULT", "Remote result cannot be sent", false, token.empty()}});
		return;
	}
	QJsonObject body{{QStringLiteral("status"), value.success ? QStringLiteral("completed") : QStringLiteral("failed")}};
	if (value.success) {
		body.insert(QStringLiteral("clip_id"), QString::fromStdString(value.clipId));
		body.insert(QStringLiteral("file_name"), QString::fromStdString(value.fileName));
		body.insert(QStringLiteral("duration_seconds"), value.durationSeconds);
		body.insert(QStringLiteral("orientation"), QString::fromStdString(value.orientation));
		body.insert(QStringLiteral("message"), QString::fromStdString(value.message));
	} else {
		body.insert(QStringLiteral("error_code"), QString::fromStdString(value.errorCode));
		body.insert(QStringLiteral("error_message"), QString::fromStdString(value.errorMessage));
	}
	const auto path = QStringLiteral("/api/plugin/remote/commands/%1/result")
			  .arg(QString::fromStdString(value.commandUuid));
	auto *reply = manager_->post(makeRequest(endpoint(path), token),
				     QJsonDocument(body).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply, completion = std::move(completion)]() mutable {
		const auto bytes = reply->readAll();
		QJsonParseError parseError;
		const auto document = QJsonDocument::fromJson(bytes, &parseError);
		const auto body = document.isObject() ? document.object() : QJsonObject{};
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
			completion({{}, replyError(*reply, body)});
		} else {
			completion({true, {}});
		}
		reply->deleteLater();
	});
}

void RemoteClipperClient::markProcessing(const std::string &uuid, const std::string &token,
					 ResultCompletion completion)
{
	if (!configured() || token.empty() || !remote::isValidCommandUuid(uuid)) {
		completion({{}, {0, "INVALID_COMMAND", "Remote command cannot be acknowledged", false, token.empty()}});
		return;
	}
	const auto path = QStringLiteral("/api/plugin/remote/commands/%1/processing")
			  .arg(QString::fromStdString(uuid));
	auto *reply = manager_->post(makeRequest(endpoint(path), token), QByteArrayLiteral("{}"));
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply, completion = std::move(completion)]() mutable {
		const auto bytes = reply->readAll();
		QJsonParseError parseError;
		const auto document = QJsonDocument::fromJson(bytes, &parseError);
		const auto body = document.isObject() ? document.object() : QJsonObject{};
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300)
			completion({{}, replyError(*reply, body)});
		else
			completion({true, {}});
		reply->deleteLater();
	});
}

} // namespace clipcoach::network
