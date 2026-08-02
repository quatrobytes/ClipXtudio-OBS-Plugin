#pragma once

#include <QString>
#include <QToolButton>

class QEvent;
class QFrame;

namespace clipcoach::ui {

class ContextHelpButton final : public QToolButton {
public:
	explicit ContextHelpButton(const QString &helpText, QWidget *parent = nullptr);
	~ContextHelpButton() override;

	void setHelpText(const QString &helpText);
	[[nodiscard]] QString helpText() const;

protected:
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void showHelpPopup();
	void hideHelpPopup();

	QString helpText_;
	QFrame *popup_ = nullptr;
};

} // namespace clipcoach::ui
