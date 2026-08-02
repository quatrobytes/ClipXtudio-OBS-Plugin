#include <clipcoach/network/qt-license-api.hpp>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslError>
#include <QSysInfo>
#include <QUuid>

#include <utility>

namespace clipcoach::network {
namespace {

using licensing::LicenseApiErrorKind;

std::optional<licensing::LicenseTimePoint> parseTime(const QJsonValue &value)
{
	if (!value.isString())
		return std::nullopt;
	const auto parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
	if (!parsed.isValid())
		return std::nullopt;
	return licensing::LicenseTimePoint(std::chrono::seconds(parsed.toSecsSinceEpoch()));
}

LicenseApiErrorKind mapError(const QString &code, int status)
{
	if (code == QStringLiteral("LICENSE_KEY_ALREADY_USED"))
		return LicenseApiErrorKind::KeyAlreadyUsed;
	if (code == QStringLiteral("LICENSE_KEY_INVALID"))
		return LicenseApiErrorKind::InvalidKey;
	if (code == QStringLiteral("SUBSCRIPTION_INACTIVE") || code == QStringLiteral("LICENSE_REVOKED"))
		return LicenseApiErrorKind::SubscriptionInactive;
	if (code == QStringLiteral("DEVICE_MISMATCH") || code == QStringLiteral("DEVICE_LIMIT_REACHED"))
		return LicenseApiErrorKind::DeviceMismatch;
	if (status == 429)
		return LicenseApiErrorKind::RateLimited;
	if (status >= 500)
		return LicenseApiErrorKind::Server;
	return LicenseApiErrorKind::InvalidResponse;
}

bool allowedLicenseUrl(const QUrl &url)
{
	if (!url.isValid() || url.host().isEmpty())
		return false;
	if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
		return true;
#ifdef CLIPX_ALLOW_INSECURE_LOCAL_API
	const auto host = url.host().toLower();
	return url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
	       (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost") ||
		host == QStringLiteral("::1"));
#else
	return false;
#endif
}

bool canTryNextEndpoint(const licensing::LicenseApiResult &result)
{
	switch (result.error.kind) {
	case LicenseApiErrorKind::Network:
	case LicenseApiErrorKind::Server:
		return true;
	default:
		return false;
	}
}

QString currentOs()
{
#if defined(Q_OS_WIN)
	return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
	return QStringLiteral("macos");
#else
	return QStringLiteral("linux");
#endif
}

} // namespace

QtLicenseApi::QtLicenseApi(QUrl baseUrl, QNetworkAccessManager *manager)
	: QtLicenseApi(QList<QUrl>{std::move(baseUrl)}, manager)
{
}

QtLicenseApi::QtLicenseApi(QList<QUrl> baseUrls, QNetworkAccessManager *manager)
	: manager_(manager)
{
	for (auto &url : baseUrls) {
		if (allowedLicenseUrl(url) && !baseUrls_.contains(url))
			baseUrls_.push_back(std::move(url));
	}
	configured_ = !baseUrls_.isEmpty();
	if (manager_ == nullptr) {
		ownedManager_ = std::make_unique<QNetworkAccessManager>();
		manager_ = ownedManager_.get();
	}
}

QtLicenseApi::~QtLicenseApi() = default;

void QtLicenseApi::activate(licensing::ActivationRequest request, Completion completion)
{
	QJsonObject body{
		{QStringLiteral("license_key"), QString::fromStdString(request.licenseKey)},
		{QStringLiteral("machine_fingerprint_hash"), QString::fromStdString(request.machineFingerprintHash)},
		{QStringLiteral("install_id"), QString::fromStdString(request.installId)},
		{QStringLiteral("app_version"), QStringLiteral(CLIPCOACH_VERSION)},
		{QStringLiteral("os"), currentOs()},
		{QStringLiteral("device_name"), QSysInfo::machineHostName()},
		{QStringLiteral("request_nonce"), QString::fromStdString(request.requestNonce)},
	};
	std::fill(request.licenseKey.begin(), request.licenseKey.end(), '\0');
	post(QStringLiteral("/api/licenses/activate"), body, std::move(completion));
}

void QtLicenseApi::refresh(licensing::RefreshRequest request, Completion completion)
{
	QJsonObject body{
		{QStringLiteral("refresh_token"), QString::fromStdString(request.refreshToken)},
		{QStringLiteral("license_token"), QString::fromStdString(request.licenseToken)},
		{QStringLiteral("machine_fingerprint_hash"), QString::fromStdString(request.machineFingerprintHash)},
		{QStringLiteral("install_id"), QString::fromStdString(request.installId)},
		{QStringLiteral("request_nonce"), QString::fromStdString(request.requestNonce)},
	};
	std::fill(request.refreshToken.begin(), request.refreshToken.end(), '\0');
	std::fill(request.licenseToken.begin(), request.licenseToken.end(), '\0');
	post(QStringLiteral("/api/licenses/refresh"), body, std::move(completion));
}

bool QtLicenseApi::configured() const noexcept
{
	return configured_;
}

void QtLicenseApi::post(const QString &path, const QJsonObject &body, Completion completion)
{
	if (!configured_) {
		if (completion) {
			completion({{},
				    {LicenseApiErrorKind::Server, "LICENSE_API_NOT_CONFIGURED",
				     "License API requires HTTPS (loopback HTTP is allowed only in local development builds)",
				     false}});
		}
		return;
	}
	postCandidate(path, body, 0, std::move(completion));
}

void QtLicenseApi::postCandidate(const QString &path, const QJsonObject &body, qsizetype candidateIndex,
				 Completion completion)
{
	QUrl endpoint = baseUrls_.at(candidateIndex);
	endpoint.setPath(path);
	QNetworkRequest request(endpoint);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	request.setRawHeader("X-Request-Id", QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
	request.setTransferTimeout(15000);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
	const auto secure = endpoint.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
	if (secure) {
		auto ssl = request.sslConfiguration();
		ssl.setProtocol(QSsl::TlsV1_2OrLater);
		request.setSslConfiguration(ssl);
	}

	auto *reply = manager_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
	if (secure) {
		QObject::connect(reply, &QNetworkReply::sslErrors, reply,
				 [reply](const QList<QSslError> &) { reply->abort(); });
	}
	QObject::connect(reply, &QNetworkReply::finished, reply,
			 [this, reply, path, body, candidateIndex, completion = std::move(completion)]() mutable {
				 auto result = parseReply(*reply);
				 reply->deleteLater();
				 if (!result.succeeded() && candidateIndex + 1 < baseUrls_.size() &&
				     canTryNextEndpoint(result)) {
					 postCandidate(path, body, candidateIndex + 1, std::move(completion));
					 return;
				 }
				 if (completion)
					 completion(std::move(result));
			 });
}

licensing::LicenseApiResult QtLicenseApi::parseReply(QNetworkReply &reply) const
{
	const auto status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const auto body = reply.readAll();
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(body, &parseError);
	const auto root = document.isObject() ? document.object() : QJsonObject{};

	if (status < 200 || status >= 300) {
		const auto error = root.value(QStringLiteral("error")).toObject();
		const auto code = error.value(QStringLiteral("code")).toString();
		const auto message = error.value(QStringLiteral("message")).toString();
		if (status == 0 || reply.error() != QNetworkReply::NoError) {
			return {{},
				{LicenseApiErrorKind::Network, code.isEmpty() ? "NETWORK_ERROR" : code.toStdString(),
				 message.isEmpty() ? reply.errorString().toStdString() : message.toStdString(), true}};
		}
		return {{},
			{mapError(code, status), code.isEmpty() ? "LICENSE_API_ERROR" : code.toStdString(),
			 message.isEmpty() ? "License API request failed" : message.toStdString(),
			 status == 429 || status >= 500}};
	}
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		return {{},
			{LicenseApiErrorKind::InvalidResponse, "INVALID_JSON_RESPONSE",
			 "License API returned invalid JSON", false}};
	}

	const auto data = root.value(QStringLiteral("data")).toObject();
	if (data.value(QStringLiteral("plan")).toString() == QStringLiteral("free") ||
	    data.value(QStringLiteral("status")).toString() == QStringLiteral("revoked")) {
		const auto reason = data.value(QStringLiteral("reason")).toObject();
		const auto code = reason.value(QStringLiteral("code")).toString();
		const auto message = data.value(QStringLiteral("message")).toString(
			reason.value(QStringLiteral("message")).toString());
		return {{},
			{LicenseApiErrorKind::SubscriptionInactive,
			 code.isEmpty() ? "SUBSCRIPTION_INACTIVE" : code.toStdString(),
			 message.isEmpty() ? "The Pro license is no longer active" : message.toStdString(), false}};
	}
	licensing::LicenseServerResponse response;
	response.licenseToken = data.value(QStringLiteral("license_token")).toString().toStdString();
	response.refreshToken = data.value(QStringLiteral("refresh_token")).toString().toStdString();
	response.status = data.value(QStringLiteral("status")).toString().toStdString();
	response.renewsAt = parseTime(data.value(QStringLiteral("renews_at")));
	if (data.value(QStringLiteral("monthly_usage")).isDouble())
		response.monthlyUsage = static_cast<std::uint32_t>(data.value(QStringLiteral("monthly_usage")).toInt());
	if (data.value(QStringLiteral("monthly_limit")).isDouble())
		response.monthlyLimit = static_cast<std::uint32_t>(data.value(QStringLiteral("monthly_limit")).toInt());
	if (response.licenseToken.empty() || response.refreshToken.empty() || response.status.empty()) {
		return {{},
			{LicenseApiErrorKind::InvalidResponse, "LICENSE_RESPONSE_INCOMPLETE",
			 "License API response is incomplete", false}};
	}
	return {std::move(response), {}};
}

} // namespace clipcoach::network
