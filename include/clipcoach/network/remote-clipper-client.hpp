#pragma once

#include <clipcoach/remote/remote-clipper-types.hpp>

#include <QUrl>

#include <functional>
#include <memory>

class QNetworkAccessManager;

namespace clipcoach::network {

class RemoteClipperApi {
public:
	using HeartbeatCompletion = std::function<void(remote::RemoteClientResult<remote::RemoteHeartbeatResponse>)>;
	using CommandsCompletion = std::function<void(remote::RemoteClientResult<std::vector<remote::RemoteCommand>>)>;
	using ResultCompletion = std::function<void(remote::RemoteClientResult<bool>)>;

	virtual ~RemoteClipperApi() = default;
	virtual void heartbeat(const remote::RemoteHeartbeatRequest &request, const std::string &bearerToken,
			       HeartbeatCompletion completion) = 0;
	virtual void commands(const std::string &deviceActivationId, const std::string &bearerToken,
			      CommandsCompletion completion) = 0;
	virtual void markProcessing(const std::string &commandUuid, const std::string &bearerToken,
				    ResultCompletion completion) = 0;
	virtual void reportResult(const remote::RemoteCommandResult &result, const std::string &bearerToken,
				  ResultCompletion completion) = 0;
};

class RemoteClipperClient final : public RemoteClipperApi {
public:
	explicit RemoteClipperClient(QUrl baseUrl, QNetworkAccessManager *manager = nullptr);
	~RemoteClipperClient() override;

	void heartbeat(const remote::RemoteHeartbeatRequest &request, const std::string &bearerToken,
		       HeartbeatCompletion completion) override;
	void commands(const std::string &deviceActivationId, const std::string &bearerToken,
		      CommandsCompletion completion) override;
	void markProcessing(const std::string &commandUuid, const std::string &bearerToken,
			    ResultCompletion completion) override;
	void reportResult(const remote::RemoteCommandResult &result, const std::string &bearerToken,
			  ResultCompletion completion) override;
	[[nodiscard]] bool configured() const noexcept;

private:
	QUrl endpoint(const QString &path) const;
	QUrl baseUrl_;
	QNetworkAccessManager *manager_{nullptr};
	std::unique_ptr<QNetworkAccessManager> ownedManager_;
};

} // namespace clipcoach::network
