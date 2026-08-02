#include <clipcoach/network/remote-command-poller.hpp>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <cassert>
#include <vector>

namespace {

class FakeApi final : public clipcoach::network::RemoteClipperApi {
public:
	clipcoach::remote::RemoteClientResult<clipcoach::remote::RemoteHeartbeatResponse> heartbeatResult;
	clipcoach::remote::RemoteClientResult<std::vector<clipcoach::remote::RemoteCommand>> commandsResult;
	int heartbeatCalls{0};
	int commandCalls{0};
	bool deferHeartbeat{false};
	std::vector<HeartbeatCompletion> pendingHeartbeats;
	std::vector<std::string> heartbeatTokens;

	void heartbeat(const clipcoach::remote::RemoteHeartbeatRequest &, const std::string &token,
		       HeartbeatCompletion completion) override
	{
		++heartbeatCalls;
		heartbeatTokens.push_back(token);
		if (deferHeartbeat) {
			pendingHeartbeats.push_back(std::move(completion));
			return;
		}
		completion(heartbeatResult);
	}
	void commands(const std::string &, const std::string &, CommandsCompletion completion) override
	{
		++commandCalls;
		completion(commandsResult);
	}
	void markProcessing(const std::string &, const std::string &, ResultCompletion completion) override
	{
		completion({true, {}});
	}
	void reportResult(const clipcoach::remote::RemoteCommandResult &, const std::string &,
			  ResultCompletion completion) override
	{
		completion({true, {}});
	}
};

clipcoach::network::RemoteCommandPoller::Providers providers(bool pro, bool local, std::string token)
{
	clipcoach::network::RemoteCommandPoller::Providers result;
	result.proActive = [pro] { return pro; };
	result.localCommandsEnabled = [local] { return local; };
	result.shuttingDown = [] { return false; };
	result.bearerToken = [token = std::move(token)] { return token; };
	result.heartbeat = [] {
		clipcoach::remote::RemoteHeartbeatRequest request;
		request.deviceActivationId = "device-1";
		return request;
	};
	return result;
}

void waitUntil(const std::function<bool()> &done)
{
	QEventLoop loop;
	QTimer timer;
	timer.setInterval(5);
	QObject::connect(&timer, &QTimer::timeout, &loop, [&] { if (done()) loop.quit(); });
	QTimer::singleShot(1000, &loop, &QEventLoop::quit);
	timer.start();
	loop.exec();
	assert(done());
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);
	{
		FakeApi api;
		api.heartbeatResult.value = clipcoach::remote::RemoteHeartbeatResponse{true, 3, "session-1", "active"};
		api.commandsResult.value = std::vector<clipcoach::remote::RemoteCommand>{
			{"123e4567-e89b-12d3-a456-426614174000", clipcoach::remote::RemoteCommandType::SaveVertical, 60, 10}};
		clipcoach::network::RemoteCommandPoller poller(api, providers(true, true, "token"));
		bool delivered = false;
		poller.setCommandsCallback([&](auto commands) { delivered = commands.size() == 1; });
		poller.start();
		waitUntil([&] { return delivered; });
		assert(api.commandCalls == 1 && poller.status().connection == clipcoach::remote::RemoteConnectionState::Connected);
		poller.stop();
	}
	{
		FakeApi api;
		api.deferHeartbeat = true;
		api.heartbeatResult.value = clipcoach::remote::RemoteHeartbeatResponse{
			true, 3, "session-1", "active"};
		api.commandsResult.value = std::vector<clipcoach::remote::RemoteCommand>{};
		std::string token = "old-token";
		auto dynamicProviders = providers(true, true, {});
		dynamicProviders.bearerToken = [&token] { return token; };
		clipcoach::network::RemoteCommandPoller poller(api, std::move(dynamicProviders));
		poller.start();
		waitUntil([&] { return api.heartbeatCalls == 1; });
		token = "new-token";
		poller.notifyCredentialsChanged();
		waitUntil([&] { return api.heartbeatCalls == 2; });
		assert(api.heartbeatTokens[0] == "old-token" &&
		       api.heartbeatTokens[1] == "new-token");
		api.pendingHeartbeats[0]({{}, {401, "LICENSE_TOKEN_REVOKED",
			"Old token revoked", false, true}});
		application.processEvents();
		assert(poller.status().connection !=
		       clipcoach::remote::RemoteConnectionState::Unauthorized);
		api.pendingHeartbeats[1](api.heartbeatResult);
		waitUntil([&] { return poller.status().connection ==
			clipcoach::remote::RemoteConnectionState::Connected; });
		poller.stop();
	}
	{
		FakeApi api;
		api.heartbeatResult.value = clipcoach::remote::RemoteHeartbeatResponse{true, 3, "session-1", "active"};
		api.commandsResult.error = {403, "TOKEN_ABILITY_MISSING",
			"The license token cannot perform this action.", false, true};
		clipcoach::network::RemoteCommandPoller poller(api, providers(true, true, "token"));
		bool reportedConnected = false;
		poller.setStatusCallback([&](const auto &status) {
			if (status.connection == clipcoach::remote::RemoteConnectionState::Connected)
				reportedConnected = true;
		});
		poller.start();
		waitUntil([&] {
			return poller.status().connection ==
			       clipcoach::remote::RemoteConnectionState::Unauthorized;
		});
		assert(!reportedConnected);
		assert(poller.status().errorCode == "TOKEN_ABILITY_MISSING");
		poller.stop();
	}
	{
		FakeApi api;
		api.heartbeatResult.error = {0, "NETWORK_ERROR", "offline", true, false};
		clipcoach::network::RemoteCommandPoller poller(api, providers(true, true, "token"));
		poller.start();
		waitUntil([&] { return poller.status().connection == clipcoach::remote::RemoteConnectionState::Offline; });
		assert(api.commandCalls == 0);
		poller.stop();
	}
	{
		FakeApi api;
		clipcoach::network::RemoteCommandPoller poller(api, providers(true, true, {}));
		poller.start();
		waitUntil([&] { return poller.status().connection == clipcoach::remote::RemoteConnectionState::Unauthorized; });
		assert(api.heartbeatCalls == 0);
		poller.stop();
	}
	{
		FakeApi api;
		api.heartbeatResult.value = clipcoach::remote::RemoteHeartbeatResponse{false, 3, {}, "Add-on inactive"};
		clipcoach::network::RemoteCommandPoller poller(api, providers(true, true, "token"));
		poller.start();
		waitUntil([&] { return api.heartbeatCalls == 1; });
		assert(api.commandCalls == 0 && !poller.status().remoteEnabled);
		poller.stop();
	}
	{
		FakeApi api;
		api.heartbeatResult.error = {403, "REMOTE_ADDON_REQUIRED", "Add-on inactive", false, true};
		clipcoach::network::RemoteCommandPoller poller(api, providers(true, true, "token"));
		poller.start();
		waitUntil([&] {
			return poller.status().connection ==
			       clipcoach::remote::RemoteConnectionState::Unauthorized;
		});
		assert(poller.status().errorCode == "REMOTE_ADDON_REQUIRED" &&
		       poller.status().message == "Add-on inactive");
		poller.stop();
	}
	return 0;
}
