#pragma once

#include <clipcoach/core/clip-manager.hpp>
#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/remote/remote-clipper-types.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class QTimer;

namespace clipcoach {
class SettingsManager;
}

namespace clipcoach::plugin {

class RemoteCaptureCoordinator final {
public:
	using Completion = std::function<void(remote::RemoteCommandResult)>;
	using ReplayCapacityCallback = std::function<void(int)>;

	RemoteCaptureCoordinator(ClipManager &clips, ExportManager &exports, SettingsManager &settings,
				 ReplayCapacityCallback ensureCapacity);
	~RemoteCaptureCoordinator();
	void capture(const remote::RemoteCapturePlan &plan, Completion completion);
	void cancel();

private:
	void clipSaved(const ClipMetadata &clip);
	void captureError(CaptureError error, const std::string &message);
	void inspectExports();
	void complete(remote::RemoteCommandResult result);
	[[nodiscard]] ExportRequest requestFor(const ClipMetadata &clip) const;

	ClipManager &clips_;
	ExportManager &exports_;
	SettingsManager &settings_;
	ReplayCapacityCallback ensureCapacity_;
	ClipManager::ObserverId savedObserver_{0};
	ClipManager::ObserverId errorObserver_{0};
	std::unique_ptr<QTimer> exportTimer_;
	std::optional<remote::RemoteCapturePlan> plan_;
	Completion completion_;
	std::vector<std::string> exportJobIds_;
};

} // namespace clipcoach::plugin
