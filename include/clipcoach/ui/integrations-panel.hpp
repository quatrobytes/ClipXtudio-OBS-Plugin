#pragma once

#include <QWidget>

#include <functional>
#include <map>

class QLabel;
class QPushButton;
class QUrl;

namespace clipcoach::integrations {
class ChatIntegrationManager;
enum class ChatPlatform;
} // namespace clipcoach::integrations

namespace clipcoach::ui {

class IntegrationsPanel final : public QWidget {
public:
	using TranslationFunction = std::function<QString(const char *)>;
	using AuthorizationOpener = std::function<bool(const QUrl &)>;

	IntegrationsPanel(TranslationFunction translator, integrations::ChatIntegrationManager *manager,
			  AuthorizationOpener opener = {}, QWidget *parent = nullptr);

	void refresh();

private:
	struct PlatformRow {
		QLabel *status{nullptr};
		QPushButton *connect{nullptr};
		QPushButton *disconnect{nullptr};
	};

	[[nodiscard]] QString text(const char *key) const;
	void addPlatform(integrations::ChatPlatform platform, const QString &name, const QString &statusObjectName);
	void beginConnect(integrations::ChatPlatform platform);
	void disconnect(integrations::ChatPlatform platform);

	TranslationFunction translator_;
	integrations::ChatIntegrationManager *manager_{nullptr};
	AuthorizationOpener opener_;
	std::map<integrations::ChatPlatform, PlatformRow> rows_;
};

} // namespace clipcoach::ui
