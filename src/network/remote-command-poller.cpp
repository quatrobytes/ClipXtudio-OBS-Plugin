#include <clipcoach/network/remote-command-poller.hpp>

#include <QTimer>
#include <QPointer>

#include <algorithm>

namespace clipcoach::network {

RemoteCommandPoller::RemoteCommandPoller(RemoteClipperApi &api, Providers providers, QObject *parent)
	: QObject(parent), api_(api), providers_(std::move(providers)), timer_(std::make_unique<QTimer>())
{
	timer_->setSingleShot(true);
	QObject::connect(timer_.get(), &QTimer::timeout, this, [this] { runCycle(); });
}

RemoteCommandPoller::~RemoteCommandPoller() { stop(); }

void RemoteCommandPoller::start()
{
	running_ = true;
	authorizationPaused_ = false;
	++credentialGeneration_;
	schedule(0);
}

void RemoteCommandPoller::stop()
{
	running_ = false;
	++credentialGeneration_;
	inFlight_ = false;
	if (timer_) timer_->stop();
	status_.connection = remote::RemoteConnectionState::Paused;
	publish();
}

void RemoteCommandPoller::pollNow()
{
	if (running_ && !inFlight_ && !authorizationPaused_) schedule(0);
}

void RemoteCommandPoller::notifyCredentialsChanged()
{
	// A refresh rotates and revokes the previous access token. Any callback
	// already in flight must not be allowed to publish its stale 401/403 after
	// the new credentials are available.
	++credentialGeneration_;
	inFlight_ = false;
	authorizationPaused_ = false;
	consecutiveFailures_ = 0;
	pollNow();
}

void RemoteCommandPoller::setCommandsCallback(CommandsCallback callback) { commandsCallback_ = std::move(callback); }
void RemoteCommandPoller::setStatusCallback(StatusCallback callback) { statusCallback_ = std::move(callback); }
remote::RemoteClipperStatus RemoteCommandPoller::status() const { return status_; }

void RemoteCommandPoller::runCycle()
{
	if (!running_ || inFlight_) return;
	if ((providers_.shuttingDown && providers_.shuttingDown()) ||
	    (providers_.proActive && !providers_.proActive()) ||
	    (providers_.localCommandsEnabled && !providers_.localCommandsEnabled())) {
		status_.connection = remote::RemoteConnectionState::Paused;
		status_.remoteEnabled = false;
		status_.localCommandsEnabled = !providers_.localCommandsEnabled || providers_.localCommandsEnabled();
		status_.message = providers_.proActive && !providers_.proActive()
				  ? "ClipXtudio Pro is required"
				  : "Remote commands are paused locally";
		publish();
		if (!providers_.shuttingDown || !providers_.shuttingDown()) schedule(5);
		return;
	}
	const auto token = providers_.bearerToken ? providers_.bearerToken() : std::string{};
	const auto heartbeat = providers_.heartbeat ? providers_.heartbeat() : remote::RemoteHeartbeatRequest{};
	if (token.empty() || heartbeat.deviceActivationId.empty()) {
		status_.connection = remote::RemoteConnectionState::Unauthorized;
		status_.message = "Remote authorization is unavailable";
		publish();
		authorizationPaused_ = true;
		return;
	}
	inFlight_ = true;
	const auto generation = credentialGeneration_;
	status_.connection = remote::RemoteConnectionState::Connecting;
	publish();
	QPointer<RemoteCommandPoller> self(this);
	api_.heartbeat(heartbeat, token, [self, token, deviceId = heartbeat.deviceActivationId, generation](auto result) {
		if (self.isNull()) return;
		if (generation != self->credentialGeneration_) return;
		if (!self->running_) { self->inFlight_ = false; return; }
		if (!result.succeeded()) { self->inFlight_ = false; self->fail(result.error); return; }
		self->consecutiveFailures_ = 0;
		self->status_.remoteEnabled = result.value->remoteEnabled;
		self->status_.sessionId = result.value->sessionId;
		self->status_.connection = self->status_.remoteEnabled && !self->status_.sessionId.empty()
			? remote::RemoteConnectionState::Connecting
			: remote::RemoteConnectionState::Unavailable;
		self->status_.message = result.value->message;
		self->status_.errorCode.clear();
		self->status_.pollIntervalSeconds = result.value->pollIntervalSeconds;
		self->status_.lastHeartbeatAt = std::chrono::system_clock::now();
		self->publish();
		if (!self->status_.remoteEnabled || self->status_.sessionId.empty()) {
			self->inFlight_ = false;
			self->schedule(self->status_.pollIntervalSeconds);
			return;
		}
		self->api_.commands(deviceId, token, [self, generation](auto commandsResult) {
			if (self.isNull()) return;
			if (generation != self->credentialGeneration_) return;
			self->inFlight_ = false;
			if (!self->running_) return;
			if (!commandsResult.succeeded()) { self->fail(commandsResult.error); return; }
			self->consecutiveFailures_ = 0;
			self->status_.connection = remote::RemoteConnectionState::Connected;
			self->status_.errorCode.clear();
			self->status_.pendingCommands = static_cast<int>(commandsResult.value->size());
			self->publish();
			if (self->commandsCallback_ && !commandsResult.value->empty())
				self->commandsCallback_(std::move(*commandsResult.value));
			self->status_.pendingCommands = 0;
			self->publish();
			self->schedule(self->status_.pollIntervalSeconds);
		});
	});
}

void RemoteCommandPoller::schedule(int seconds)
{
	if (!running_ || authorizationPaused_ || !timer_) return;
	timer_->start(std::max(0, seconds) * 1000);
}

void RemoteCommandPoller::fail(const remote::RemoteClientError &error)
{
	++consecutiveFailures_;
	status_.message = error.message.empty() ? "Remote Clipper network request failed" : error.message;
	status_.errorCode = error.code;
	if (error.unauthorized) {
		status_.connection = remote::RemoteConnectionState::Unauthorized;
		status_.remoteEnabled = false;
		authorizationPaused_ = true;
	} else {
		status_.connection = remote::RemoteConnectionState::Offline;
		const int backoff = std::min(60, 3 * (1 << std::min(consecutiveFailures_ - 1, 4)));
		schedule(backoff);
	}
	publish();
}

void RemoteCommandPoller::publish()
{
	if (statusCallback_) statusCallback_(status_);
}

} // namespace clipcoach::network
