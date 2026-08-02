#pragma once

#include <QFrame>

namespace clipcoach::ui {

class UpgradeBanner final : public QFrame {
public:
	explicit UpgradeBanner(const QString &title, const QString &description,
			       const QString &buttonText, const QString &badgeText,
			       QWidget *parent = nullptr);
};

} // namespace clipcoach::ui
