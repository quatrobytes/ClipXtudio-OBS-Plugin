#pragma once

#include <QPushButton>

namespace clipcoach::ui {

class PrimaryButton final : public QPushButton {
public:
	explicit PrimaryButton(const QString &text, QWidget *parent = nullptr);
};

} // namespace clipcoach::ui
