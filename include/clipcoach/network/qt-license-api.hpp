#pragma once

#include <clipcoach/licensing/license-api.hpp>

#include <QList>
#include <QUrl>

#include <memory>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;

namespace clipcoach::network {

class QtLicenseApi final : public licensing::LicenseApi {
public:
	explicit QtLicenseApi(QUrl baseUrl, QNetworkAccessManager *manager = nullptr);
	explicit QtLicenseApi(QList<QUrl> baseUrls, QNetworkAccessManager *manager = nullptr);
	~QtLicenseApi() override;

	void activate(licensing::ActivationRequest request, Completion completion) override;
	void refresh(licensing::RefreshRequest request, Completion completion) override;

	[[nodiscard]] bool configured() const noexcept;

private:
	void post(const QString &path, const QJsonObject &body, Completion completion);
	void postCandidate(const QString &path, const QJsonObject &body, qsizetype candidateIndex,
			   Completion completion);
	[[nodiscard]] licensing::LicenseApiResult parseReply(QNetworkReply &reply) const;

	QList<QUrl> baseUrls_;
	std::unique_ptr<QNetworkAccessManager> ownedManager_;
	QNetworkAccessManager *manager_{nullptr};
	bool configured_{false};
};

} // namespace clipcoach::network
