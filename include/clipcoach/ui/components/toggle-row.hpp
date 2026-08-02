#pragma once

#include <QWidget>

class QCheckBox;

namespace clipcoach::ui {

class ContextHelpButton;

class ToggleRow final : public QWidget {
public:
	explicit ToggleRow(const QString &title, const QString &description, bool checked,
			   QWidget *parent = nullptr);

	[[nodiscard]] bool isChecked() const;
	void setChecked(bool checked);
	void setHelpButtonObjectName(const QString &objectName);

private:
	QCheckBox *toggle_{nullptr};
	ContextHelpButton *helpButton_{nullptr};
};

} // namespace clipcoach::ui
