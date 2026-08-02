#pragma once

#include <clipcoach/network/remote-clipper-client.hpp>

#include <QObject>

#include <functional>
#include <cstdint>
#include <memory>

class QTimer;

namespace clipcoach::network {

class RemoteCommandPoller final : public QObject {
public:
	struct Providers {
		std::function<bool()> proActive;
		std::function<bool()> localCommandsEnabled;
		std::function<bool()> shuttingDown;
		std::function<std::string()> bearerToken;
		std::function<remote::RemoteHeartbeatRequest()> heartbeat;
	};
	using CommandsCallback = std::function<void(std::vector<remote::RemoteCommand>)>;
	using StatusCallback = std::function<void(const remote::RemoteClipperStatus &)>;

	RemoteCommandPoller(RemoteClipperApi &api, Providers providers, QObject *parent = nullptr);
	~RemoteCommandPoller() override;
	void start();
	void stop();
	void pollNow();
	void notifyCredentialsChanged();
	void setCommandsCallback(CommandsCallback callback);
	void setStatusCallback(StatusCallback callback);
	[[nodiscard]] remote::RemoteClipperStatus status() const;

private:
	void runCycle();
	void schedule(int seconds);
	void fail(const remote::RemoteClientError &error);
	void publish();

	RemoteClipperApi &api_;
	Providers providers_;
	std::unique_ptr<QTimer> timer_;
	CommandsCallback commandsCallback_;
	StatusCallback statusCallback_;
	remote::RemoteClipperStatus status_;
	bool running_{false};
	bool inFlight_{false};
	bool authorizationPaused_{false};
	int consecutiveFailures_{0};
	std::uint64_t credentialGeneration_{0};
};

} // namespace clipcoach::network
