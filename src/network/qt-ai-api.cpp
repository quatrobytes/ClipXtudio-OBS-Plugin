#include <clipcoach/network/qt-ai-api.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QTimer>

namespace clipcoach::network {
namespace {

bool allowedBackendUrl(const QUrl &url)
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

AiAssistantResult fail(AiError error, std::string code, std::string message)
{
	return {false, error, std::move(code), std::move(message)};
}

std::vector<std::string> strings(const QJsonValue &value)
{
	std::vector<std::string> result;
	if (!value.isArray())
		return result;
	for (const auto item : value.toArray())
		if (item.isString())
			result.push_back(item.toString().toStdString());
	return result;
}

AiAssistantResult parse(QNetworkReply *reply)
{
	const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const auto document = QJsonDocument::fromJson(reply->readAll());
	const auto root = document.object();
	const auto errorObject = root.value(QStringLiteral("error")).toObject();
	const auto serverCode = errorObject.value(QStringLiteral("code")).toString();
	const auto serverMessage = errorObject.value(QStringLiteral("message")).toString();
	if (status == 402 || status == 429) {
		return fail(AiError::UsageLimit,
			    serverCode.isEmpty() ? std::string{"AI_USAGE_LIMIT"} : serverCode.toStdString(),
			    serverMessage.isEmpty() ? std::string{"Monthly AI credits exhausted"}
						    : serverMessage.toStdString());
	}
	if (status == 401 || status == 403) {
		return fail(AiError::ProRequired,
			    serverCode.isEmpty() ? std::string{"PRO_REQUIRED"} : serverCode.toStdString(),
			    serverMessage.isEmpty() ? std::string{"An active ClipXtudio Pro license is required"}
						    : serverMessage.toStdString());
	}
	if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
		return fail(
			AiError::Network,
			serverCode.isEmpty() ? std::string{"AI_NETWORK_ERROR"} : serverCode.toStdString(),
			serverMessage.isEmpty()
				? std::string{"The AI service could not complete this request. Try again in a moment."}
				: serverMessage.toStdString());
	}
	const auto data = root.value(QStringLiteral("data")).toObject();
	if (data.isEmpty())
		return fail(AiError::InvalidResponse, "AI_INVALID_RESPONSE", "AI backend returned invalid JSON");
	AiAssistantResponse response;
	response.suggestedTitles = strings(data.value(QStringLiteral("suggested_titles")));
	response.caption = data.value(QStringLiteral("caption")).toString().toStdString();
	response.hashtags = strings(data.value(QStringLiteral("hashtags")));
	response.summary = data.value(QStringLiteral("summary")).toString().toStdString();
	if (data.value(QStringLiteral("quality_score")).isDouble())
		response.qualityScore = data.value(QStringLiteral("quality_score")).toInt(-1);
	if (data.value(QStringLiteral("hook_strength")).isDouble())
		response.hookStrength = data.value(QStringLiteral("hook_strength")).toInt(-1);
	response.qualityReason = data.value(QStringLiteral("quality_reason")).toString().toStdString();
	if (data.value(QStringLiteral("srt")).isString())
		response.srt = data.value(QStringLiteral("srt")).toString().toStdString();
	if (data.value(QStringLiteral("vtt")).isString())
		response.vtt = data.value(QStringLiteral("vtt")).toString().toStdString();
	for (const auto item : data.value(QStringLiteral("subtitle_cues")).toArray()) {
		const auto cue = item.toObject();
		response.subtitleCues.push_back(
			{static_cast<std::int64_t>(cue.value(QStringLiteral("start_ms")).toDouble()),
			 static_cast<std::int64_t>(cue.value(QStringLiteral("end_ms")).toDouble()),
			 cue.value(QStringLiteral("text")).toString().toStdString()});
	}
	const auto usage = data.value(QStringLiteral("usage")).toObject();
	response.usage = static_cast<std::uint32_t>(usage.value(QStringLiteral("used")).toInt());
	response.limit = static_cast<std::uint32_t>(usage.value(QStringLiteral("limit")).toInt());
	return {true, AiError::None, {}, {}, std::move(response)};
}

} // namespace

QtAiApi::QtAiApi(QUrl baseUrl) : QtAiApi(QList<QUrl>{std::move(baseUrl)}) {}
QtAiApi::QtAiApi(QList<QUrl> baseUrls)
{
	for (auto &url : baseUrls) {
		if (allowedBackendUrl(url) && !baseUrls_.contains(url))
			baseUrls_.push_back(std::move(url));
	}
}
QtAiApi::~QtAiApi()
{
	cancelAll();
}

void QtAiApi::analyze(AiAssistantRequest request, std::string authorizationToken, Callback callback)
{
	analyzeAt(0, std::move(request), std::move(authorizationToken), std::move(callback));
}

void QtAiApi::analyzeAt(std::size_t index, AiAssistantRequest request, std::string authorizationToken,
			Callback callback)
{
	if (authorizationToken.empty()) {
		callback(fail(AiError::ProRequired, "AI_AUTHORIZATION_REQUIRED",
			      "AI authorization is unavailable. Refresh the Pro license and try again."));
		return;
	}
	if (index >= static_cast<std::size_t>(baseUrls_.size())) {
		callback(fail(AiError::Network, "AI_BACKEND_UNAVAILABLE", "No secure AI backend is configured."));
		return;
	}
	QUrl url = baseUrls_.at(static_cast<qsizetype>(index));
	url.setPath(QStringLiteral("/api/ai/analyze"));
	QNetworkRequest networkRequest(url);
	networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	networkRequest.setRawHeader("Authorization",
				    QByteArray("Bearer ") + QByteArray::fromStdString(authorizationToken));
	networkRequest.setRawHeader("X-Request-Id", QByteArray::fromStdString(request.requestId));
	networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
	// The backend permits providers to take up to 120 seconds. Keep the client
	// alive slightly longer so a valid slow inference is not aborted first.
	networkRequest.setTransferTimeout(125'000);
	auto tls = QSslConfiguration::defaultConfiguration();
	tls.setProtocol(QSsl::TlsV1_2OrLater);
	networkRequest.setSslConfiguration(tls);
	QJsonObject body{
		{QStringLiteral("scope"),
		 request.scope == AiRequestScope::Clip ? QStringLiteral("clip") : QStringLiteral("session")},
		{QStringLiteral("clip_id"), QString::fromStdString(request.clipId)},
		{QStringLiteral("session_id"), QString::fromStdString(request.sessionId)},
		{QStringLiteral("transcript"), QString::fromStdString(request.transcript)},
		{QStringLiteral("language"), QString::fromLatin1(AiAssistantService::languageCode(request.language))},
	};
	auto *reply = manager_.post(networkRequest, QJsonDocument(body).toJson(QJsonDocument::Compact));
	active_.insert(reply);
	auto *timeout = new QTimer(reply);
	timeout->setSingleShot(true);
	timeout->setInterval(125'000);
	QObject::connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
	QObject::connect(reply, &QNetworkReply::sslErrors, reply,
			 [reply](const QList<QSslError> &) { reply->abort(); });
	QObject::connect(reply, &QNetworkReply::finished, reply,
			 [this, reply, index, request = std::move(request),
			  authorizationToken = std::move(authorizationToken),
			  callback = std::move(callback)]() mutable {
				 active_.erase(reply);
				 const auto result = parse(reply);
				 reply->deleteLater();
				 if (!result.success && result.error == AiError::Network &&
				     index + 1 < static_cast<std::size_t>(baseUrls_.size())) {
					 analyzeAt(index + 1, std::move(request), std::move(authorizationToken),
						   std::move(callback));
					 return;
				 }
				 callback(result);
			 });
	timeout->start();
}

void QtAiApi::cancelAll() noexcept
{
	const auto replies = active_;
	for (auto *reply : replies)
		if (reply)
			reply->abort();
	active_.clear();
}

} // namespace clipcoach::network
