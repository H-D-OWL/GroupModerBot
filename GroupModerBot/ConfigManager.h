#pragma once

#include <filesystem>
#include <string>
#include <string_view> 

#include "Logging.h"

namespace gmb
{
	class ConfigManager final
	{
	public:

		ConfigManager(const ConfigManager&) = delete;
		ConfigManager& operator=(const ConfigManager&) = delete;
		ConfigManager(ConfigManager&&) noexcept = delete;
		ConfigManager& operator=(ConfigManager&&) noexcept = delete;

		ConfigManager(const std::string_view configFile);
		~ConfigManager() = default;

		const std::filesystem::path& GetDbPath() const noexcept;
		std::string ExtractBotToken();
		bool GetEnableProcessPendingUpdates() const noexcept;
		gmb::logging::LogMode GetLogMode() const noexcept;
		size_t GetMaxLogFileSize() const noexcept;
		std::filesystem::path ExtractLogDirectory();

	private:

		std::filesystem::path FixPath(std::string path);

		bool DbPathValid(const std::filesystem::path& path) const;

		std::filesystem::path dbPath{};

		std::string botToken{};

		bool enableProcessPendingUpdates{ true };

		gmb::logging::LogMode logMode{ gmb::logging::LogMode::Error };
		size_t maxFileSizeBytes{};
		std::filesystem::path logDirectory{ };
	};
}