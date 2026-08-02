#include <clipcoach/remote/remote-result-outbox.hpp>
#include <QCoreApplication>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv); QTemporaryDir dir; if (!dir.isValid()) return 1;
	clipcoach::remote::RemoteCommandResult value;
	value.commandUuid="123e4567-e89b-12d3-a456-426614174000"; value.success=true;
	value.fileName="clip.mp4"; value.durationSeconds=60; value.orientation="vertical";
	const auto path=dir.filePath("outbox.ini");
	{ clipcoach::remote::RemoteResultOutbox outbox(path); if (!outbox.enqueue(value)) return 2; }
	{ clipcoach::remote::RemoteResultOutbox outbox(path); auto pending=outbox.pending();
	  if (pending.size()!=1 || pending[0].fileName!="clip.mp4") return 3;
	  if (!outbox.remove(value.commandUuid) || !outbox.pending().empty()) return 4; }
	return 0;
}
