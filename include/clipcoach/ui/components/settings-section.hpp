#pragma once

#include <QFrame>

class QVBoxLayout;

namespace clipcoach::ui {

class ContextHelpButton;

class SettingsSection final : public QFrame {
public:
	explicit SettingsSection(const QString &title, QWidget *parent = nullptr);

	[[nodiscard]] QVBoxLayout *contentLayout() const noexcept;
	void setHelpText(const QString &helpText);

private:
	QVBoxLayout *contentLayout_{nullptr};
	ContextHelpButton *helpButton_{nullptr};
};

} // namespace clipcoach::ui
