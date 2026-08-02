#pragma once

#include <clipcoach/core/settings.hpp>

#include <QObject>

#include <functional>

namespace clipcoach {
class SettingsManager;
}

namespace clipcoach::ui {

class SettingsController final : public QObject {
public:
	using AppliedCallback =
		std::function<void(const Settings &before, const Settings &after)>;
	using ErrorCallback = std::function<void(const std::string &)>;

	explicit SettingsController(SettingsManager *manager,
				    QObject *parent = nullptr);

	[[nodiscard]] const Settings &settings() const noexcept;
	bool update(const std::function<void(Settings &)> &mutation);
	void setAppliedCallback(AppliedCallback callback);
	void setErrorCallback(ErrorCallback callback);

private:
	SettingsManager *manager_{nullptr};
	Settings fallback_{Settings::defaults()};
	AppliedCallback appliedCallback_;
	ErrorCallback errorCallback_;
};

} // namespace clipcoach::ui
