#pragma once

#include <clipcoach/remote/remote-clipper-types.hpp>
#include <QString>
#include <memory>
#include <vector>

class QSettings;

namespace clipcoach::remote {

class RemoteResultOutbox {
public:
	explicit RemoteResultOutbox(const QString &filePath);
	~RemoteResultOutbox();
	bool enqueue(const RemoteCommandResult &result);
	bool remove(const std::string &commandUuid);
	[[nodiscard]] std::vector<RemoteCommandResult> pending() const;

private:
	std::unique_ptr<QSettings> settings_;
};

} // namespace clipcoach::remote
