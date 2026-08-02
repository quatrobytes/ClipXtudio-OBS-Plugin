#include <clipcoach/remote/remote-result-outbox.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace clipcoach::remote {
namespace {
QJsonObject encode(const RemoteCommandResult &v)
{
	return {{"command_uuid", QString::fromStdString(v.commandUuid)},
		{"success", v.success},
		{"clip_id", QString::fromStdString(v.clipId)},
		{"file_name", QString::fromStdString(v.fileName)},
		{"duration_seconds", v.durationSeconds},
		{"orientation", QString::fromStdString(v.orientation)},
		{"message", QString::fromStdString(v.message)},
		{"error_code", QString::fromStdString(v.errorCode)},
		{"error_message", QString::fromStdString(v.errorMessage)}};
}
RemoteCommandResult decode(const QJsonObject &o)
{
	RemoteCommandResult v;
	v.commandUuid = o.value("command_uuid").toString().toStdString();
	v.success = o.value("success").toBool();
	v.clipId = o.value("clip_id").toString().toStdString();
	v.fileName = o.value("file_name").toString().toStdString();
	v.durationSeconds = o.value("duration_seconds").toInt();
	v.orientation = o.value("orientation").toString().toStdString();
	v.message = o.value("message").toString().toStdString();
	v.errorCode = o.value("error_code").toString().toStdString();
	v.errorMessage = o.value("error_message").toString().toStdString();
	return v;
}
} // namespace
RemoteResultOutbox::RemoteResultOutbox(const QString &path)
	: settings_(std::make_unique<QSettings>(path, QSettings::IniFormat))
{
}
RemoteResultOutbox::~RemoteResultOutbox() = default;
std::vector<RemoteCommandResult> RemoteResultOutbox::pending() const
{
	std::vector<RemoteCommandResult> values;
	const auto doc = QJsonDocument::fromJson(settings_->value("result_outbox").toByteArray());
	for (const auto &item : doc.array())
		if (item.isObject()) {
			auto v = decode(item.toObject());
			if (isValidCommandUuid(v.commandUuid))
				values.push_back(std::move(v));
		}
	return values;
}
bool RemoteResultOutbox::enqueue(const RemoteCommandResult &result)
{
	if (!isValidCommandUuid(result.commandUuid))
		return false;
	auto values = pending();
	bool replaced = false;
	for (auto &value : values)
		if (value.commandUuid == result.commandUuid) {
			value = result;
			replaced = true;
			break;
		}
	if (!replaced) {
		if (values.size() >= 500)
			return false;
		values.push_back(result);
	}
	QJsonArray array;
	for (const auto &value : values)
		array.push_back(encode(value));
	settings_->setValue("result_outbox", QJsonDocument(array).toJson(QJsonDocument::Compact));
	settings_->sync();
	return settings_->status() == QSettings::NoError;
}
bool RemoteResultOutbox::remove(const std::string &uuid)
{
	auto values = pending();
	const auto old = values.size();
	values.erase(std::remove_if(values.begin(), values.end(), [&](const auto &v) { return v.commandUuid == uuid; }),
		     values.end());
	QJsonArray array;
	for (const auto &value : values)
		array.push_back(encode(value));
	settings_->setValue("result_outbox", QJsonDocument(array).toJson(QJsonDocument::Compact));
	settings_->sync();
	return old != values.size() && settings_->status() == QSettings::NoError;
}
} // namespace clipcoach::remote
