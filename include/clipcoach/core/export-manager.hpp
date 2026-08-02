#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace clipcoach {

class FeatureGateService;

enum class ExportOrientation {
	Horizontal,
	Vertical,
	Both,
};

enum class ExportQualityPreset {
	Low,
	Medium,
	High,
	Maximum,
};

enum class ExportJobState {
	Pending,
	Exporting,
	Done,
	Error,
	Cancelled,
};

enum class VerticalExportSource {
	CenterCrop,
	VerticalCanvas,
};

struct ExportRequest {
	struct Segment {
		long long startMilliseconds{0};
		long long durationMilliseconds{0};
	};
	std::string clipId;
	std::filesystem::path sourcePath;
	std::filesystem::path outputDirectory;
	std::string outputBaseName;
	ExportOrientation orientation{ExportOrientation::Horizontal};
	ExportQualityPreset preset{ExportQualityPreset::High};
	int durationSeconds{0};
	int endOffsetSeconds{0};
	long long trimStartMilliseconds{-1};
	long long trimDurationMilliseconds{0};
	std::vector<Segment> keepSegments;
	int outputFps{0};
	bool preserveSourceFrame{false};
	int verticalWidth{1080};
	int verticalHeight{1920};
	VerticalExportSource verticalSource{VerticalExportSource::CenterCrop};
	std::filesystem::path verticalCanvasPath;
};

struct ExportJob {
	std::string id;
	ExportRequest request;
	ExportOrientation orientation{ExportOrientation::Horizontal};
	ExportJobState state{ExportJobState::Pending};
	int progressPercent{0};
	std::filesystem::path outputPath;
	std::string error;
	std::chrono::system_clock::time_point createdAt;
	std::chrono::system_clock::time_point updatedAt;
};

struct ExportBackendResult {
	bool success{false};
	bool cancelled{false};
	std::string error;

	[[nodiscard]] static ExportBackendResult ok() { return {true, false, {}}; }
	[[nodiscard]] static ExportBackendResult fail(std::string message)
	{
		return {false, false, std::move(message)};
	}
	[[nodiscard]] static ExportBackendResult cancelledResult() { return {false, true, "export cancelled"}; }
};

class ExportBackend {
public:
	using ProgressCallback = std::function<void(int percent)>;

	virtual ~ExportBackend() = default;
	[[nodiscard]] virtual ExportBackendResult execute(const ExportJob &job,
							  const std::filesystem::path &temporaryPath,
							  ProgressCallback progress,
							  const std::atomic_bool &cancelRequested) = 0;
};

class ExportManager final {
public:
	using JobCallback = std::function<void(const ExportJob &)>;
	using LogCallback = std::function<void(bool error, const std::string &message)>;

	explicit ExportManager(std::unique_ptr<ExportBackend> backend, JobCallback metadataCallback = {},
			       LogCallback logCallback = {}, const FeatureGateService *featureGates = nullptr);
	~ExportManager();

	ExportManager(const ExportManager &) = delete;
	ExportManager &operator=(const ExportManager &) = delete;

	[[nodiscard]] std::vector<std::string> enqueue(ExportRequest request, std::string *error = nullptr);
	[[nodiscard]] std::vector<std::string> enqueueBatch(std::vector<ExportRequest> requests,
							    std::string *error = nullptr);
	[[nodiscard]] bool cancel(const std::string &jobId, std::string *error = nullptr);
	[[nodiscard]] ExportJob job(const std::string &jobId) const;
	[[nodiscard]] std::vector<ExportJob> jobs() const;
	[[nodiscard]] bool waitUntilIdle(std::chrono::milliseconds timeout);

	void setJobCallback(JobCallback callback);

	[[nodiscard]] static bool isValidPreset(ExportQualityPreset preset) noexcept;
	[[nodiscard]] static std::string cleanBaseName(const std::string &value);
	[[nodiscard]] static std::filesystem::path createAvailableOutputPath(const std::filesystem::path &directory,
									     const std::string &baseName,
									     ExportOrientation orientation);

private:
	struct QueuedJob {
		std::string id;
		std::shared_ptr<std::atomic_bool> cancelRequested;
	};

	[[nodiscard]] bool validate(const ExportRequest &request, std::string *error) const;
	[[nodiscard]] std::string addJob(const ExportRequest &request, ExportOrientation orientation);
	void run();
	void process(const QueuedJob &queued);
	void update(const std::string &id, ExportJobState state, int progress, std::string error = {});
	void publish(const ExportJob &job);
	void log(bool error, const std::string &message) const;

	std::unique_ptr<ExportBackend> backend_;
	mutable std::mutex mutex_;
	std::condition_variable condition_;
	std::condition_variable idleCondition_;
	std::vector<QueuedJob> queue_;
	std::map<std::string, ExportJob> jobs_;
	std::map<std::string, std::shared_ptr<std::atomic_bool>> cancelFlags_;
	std::vector<std::filesystem::path> reservedPaths_;
	JobCallback metadataCallback_;
	LogCallback logCallback_;
	const FeatureGateService *featureGates_{nullptr};
	std::thread worker_;
	bool stopping_{false};
	bool active_{false};
};

} // namespace clipcoach
