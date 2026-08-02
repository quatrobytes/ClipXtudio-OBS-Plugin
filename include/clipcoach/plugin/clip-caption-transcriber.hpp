#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace clipcoach::plugin {

struct ClipTranscriptionResult {
	bool success{false};
	std::string transcript;
	std::string error;
};

class ClipCaptionTranscriber final {
public:
	using Completion = std::function<void(ClipTranscriptionResult)>;
	using Progress = std::function<void(int)>;

	ClipCaptionTranscriber(std::filesystem::path ffmpegExecutable,
			      std::filesystem::path whisperModel);
	~ClipCaptionTranscriber();

	ClipCaptionTranscriber(const ClipCaptionTranscriber &) = delete;
	ClipCaptionTranscriber &
	operator=(const ClipCaptionTranscriber &) = delete;

	void transcribe(std::filesystem::path mediaPath,
			std::string language, Progress progress,
			Completion completion);

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace clipcoach::plugin
