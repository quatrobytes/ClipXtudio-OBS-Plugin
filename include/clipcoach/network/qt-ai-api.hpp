#pragma once

#include <clipcoach/core/ai-assistant.hpp>

#include <QNetworkAccessManager>
#include <QList>
#include <QUrl>

#include <set>

class QNetworkReply;

namespace clipcoach::network {

class QtAiApi final : public AiApi {
public:
	explicit QtAiApi(QUrl baseUrl);
	explicit QtAiApi(QList<QUrl> baseUrls);
	~QtAiApi() override;

	void analyze(AiAssistantRequest request, std::string authorizationToken, Callback callback) override;
	void cancelAll() noexcept override;

private:
	void analyzeAt(std::size_t index, AiAssistantRequest request,
		       std::string authorizationToken, Callback callback);

	QList<QUrl> baseUrls_;
	QNetworkAccessManager manager_;
	std::set<QNetworkReply *> active_;
};

} // namespace clipcoach::network
