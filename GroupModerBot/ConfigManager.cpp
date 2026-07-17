#include "ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef> 
#include <filesystem>
#include <fstream>
#include <ios> 
#include <memory> 
#include <stdexcept>
#include <string>
#include <string_view> 
#include <system_error>
#include <unordered_map>
#include <utility>
#include <format>

#include "Logging.h"

namespace gmb
{
	namespace
	{
		constexpr std::string_view TrimEdges(std::string_view sv) noexcept
		{
			sv.remove_prefix(std::min(sv.find_first_not_of(" \t\n\r"), sv.size()));

			return sv.empty() ? sv : sv.substr(0, sv.find_last_not_of(" \t\n\r") + 1);
		}
	}

	ConfigManager::ConfigManager(const std::string_view configFile)
	{
		static constexpr std::string_view dbPathKey{ "DbPath=" };
		static constexpr std::string_view botTokenKey{ "BotToken=" };
		static constexpr std::string_view enableProcessPendingUpdatesKey{ "EnableProcessPendingUpdates=" };
		static constexpr std::string_view logModeKey{ "LogMode=" };
		static constexpr std::string_view maxLogFileSizeKey{ "MaxLogFileSize=" };
		static constexpr std::string_view logDirectoryKey{ "LogDirectory=" };

		static const std::unordered_map<std::string_view, bool> valuesEnableProcessPendingUpdatesKey{
			{"1", true},
			{"t", true},
			{"true", true},
			{"0", false},
			{"f", false},
			{"false", false},
		};

		//

		std::ifstream fileDataForBot(std::filesystem::path(configFile), std::ios_base::in);

		if (!fileDataForBot.is_open())
			throw std::runtime_error{ std::format("file \"{}\" not found", configFile) };

		gmb::logging::Logger::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::Event, std::format("file \"{}\" found", configFile));

		//

		std::string fileLine{};
		bool logToFile{ false };

		std::string_view valuesEnableProcessPendingUpdatesKeyLine{};
		std::string logModeLine{};
		std::string maxFileSizeBytesLine{};

		while (std::getline(fileDataForBot, fileLine))
		{
			std::erase(fileLine, '\r');

			if (fileLine.empty()) continue;

			const std::string_view lineView = fileLine;

			if (lineView.starts_with(dbPathKey))
			{
				dbPath = FixPath(std::string(TrimEdges(lineView.substr(dbPathKey.size()))));
			}
			else if (lineView.starts_with(botTokenKey))
			{
				botToken = (TrimEdges(lineView.substr(botTokenKey.size())));
			}
			else if (lineView.starts_with(enableProcessPendingUpdatesKey))
			{
				valuesEnableProcessPendingUpdatesKeyLine = TrimEdges(lineView.substr(enableProcessPendingUpdatesKey.size()));

				const auto it = valuesEnableProcessPendingUpdatesKey.find(valuesEnableProcessPendingUpdatesKeyLine);

				if (it == valuesEnableProcessPendingUpdatesKey.cend())
					throw std::runtime_error{ std::format("value EnableProcessPendingUpdatesKey is invalid (\"{}\")", valuesEnableProcessPendingUpdatesKeyLine) };

				enableProcessPendingUpdates = it->second;
			}
			else if (lineView.starts_with(logModeKey))
			{
				logModeLine = TrimEdges(lineView.substr(logModeKey.size()));

				logMode = gmb::logging::ToLogMode(logModeLine);

				logToFile = logMode == gmb::logging::LogMode::File || logMode == gmb::logging::LogMode::ConsoleAndFile;
			}
			else if (lineView.starts_with(maxLogFileSizeKey))
			{
				maxFileSizeBytesLine = TrimEdges(lineView.substr(maxLogFileSizeKey.size()));

				std::from_chars_result res = std::from_chars(maxFileSizeBytesLine.data(), maxFileSizeBytesLine.data() + maxFileSizeBytesLine.size(), maxFileSizeBytes);

				if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range || res.ptr != maxFileSizeBytesLine.data() + maxFileSizeBytesLine.size())
					maxFileSizeBytes = static_cast<size_t>(0);
			}
			else if (lineView.starts_with(logDirectoryKey))
			{
				logDirectory = FixPath(std::string(TrimEdges(lineView.substr(logDirectoryKey.size()))));
			}
		}

		fileDataForBot.close();

		//

		if (!DbPathValid(dbPath))
			throw std::runtime_error{ std::format("value DbPath is invalid (\"{}\")", dbPath.string()) };

		if (botToken.empty())
			throw std::runtime_error{ std::format("value BotToken is invalid (\"{}\")", botToken) };

		if (logMode == gmb::logging::LogMode::Error)
			throw std::runtime_error{ std::format("value LogMode is invalid (\"{}\")", logModeLine)};

		if (logToFile && maxFileSizeBytes == 0)
			throw std::runtime_error{ std::format("value MaxLogFileSize is invalid (\"{}\")", maxFileSizeBytesLine) };

		if (logToFile && (logDirectory.empty() || logDirectory.is_relative()))
			throw std::runtime_error{ std::format("value LogDirectory is invalid (\"{}\")", logDirectory.string()) };
	}

	const std::filesystem::path& ConfigManager::GetDbPath() const noexcept
	{
		return dbPath;
	}

	std::string ConfigManager::ExtractBotToken()
	{
		return std::move(botToken);
	}

	bool ConfigManager::GetEnableProcessPendingUpdates() const noexcept
	{
		return enableProcessPendingUpdates;
	}

	gmb::logging::LogMode ConfigManager::GetLogMode() const noexcept
	{
		return logMode;
	}

	size_t ConfigManager::GetMaxLogFileSize() const noexcept
	{
		return maxFileSizeBytes;
	}

	std::filesystem::path ConfigManager::ExtractLogDirectory()
	{
		return std::move(logDirectory);
	}

	///

	std::filesystem::path ConfigManager::FixPath(std::string path) {

		std::replace(path.begin(), path.end(), '\\', '/');

#ifdef __linux__ // Linux

		static constexpr const std::string_view wslPrefixs[]{ "//wsl.localhost/", "//wsl$/" };

		for (const std::string_view wslPrefix : wslPrefixs)
		{
			if (path.starts_with(wslPrefix))
			{
				if (const size_t slashPos = path.find('/', wslPrefix.size()); slashPos != wslPrefix.npos)
					path.erase(0, slashPos);

				break;
			}
		}
#endif

		return std::filesystem::path(std::move(path)).lexically_normal();
	}

	bool ConfigManager::DbPathValid(const std::filesystem::path& path) const
	{
		if (path.empty() || !path.has_extension() || path.is_relative())
			return false;

		static constexpr const std::string_view dbExtension{ ".db" };

		return dbExtension == [&path]() -> std::string
			{
				std::string extension{ path.extension().string() };

				std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

				return extension;
			}();
	}
}