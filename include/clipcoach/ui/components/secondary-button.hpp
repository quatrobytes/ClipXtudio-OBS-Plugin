#pragma once

#include <QPushButton>

namespace clipcoach::ui {

class SecondaryButton final : public QPushButton {
public:
	explicit SecondaryButton(const QString &text, QWidget *parent = nullptr);
};

} // namespace clipcoach::ui
