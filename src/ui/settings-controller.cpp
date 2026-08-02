#include <clipcoach/ui/settings-controller.hpp>

#include <clipcoach/core/settings-manager.hpp>

#include <utility>

namespace clipcoach::ui {

SettingsController::SettingsController(SettingsManager *manager, QObject *parent)
	: QObject(parent), manager_(manager)
{
}

const Settings &SettingsController::settings() const noexcept
{
	return manager_ != nullptr ? manager_->settings() : fallback_;
}

bool SettingsController::update(
	const std::function<void(Settings &)> &mutation)
{
	if (manager_ == nullptr) {
		if (errorCallback_) {
			errorCallback_("settings storage is unavailable");
		}
		return false;
	}
	const auto before = manager_->settings();
	auto candidate = before;
	mutation(candidate);
	std::string error;
	if (!manager_->save(candidate, &error)) {
		if (errorCallback_) {
			errorCallback_(error);
		}
		return false;
	}
	if (appliedCallback_) {
		appliedCallback_(before, manager_->settings());
	}
	return true;
}

void SettingsController::setAppliedCallback(AppliedCallback callback)
{
	appliedCallback_ = std::move(callback);
}

void SettingsController::setErrorCallback(ErrorCallback callback)
{
	errorCallback_ = std::move(callback);
}

} // namespace clipcoach::ui
