#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string> 
#include <string_view> 
#include <thread>
#include <vector>

#include <tgbot/types/ChatMemberUpdated.h> 
#include <tgbot/types/Message.h> 

namespace gmb
{
	namespace logging
	{
		enum class LogSource
		{
			Program,
			Database,
			Bot,
		};

		enum class LogType
		{
			Event,
			Command,
			Error,
			FatalError,
		};

		enum class LogMode
		{
			Console,
			File,
			ConsoleAndFile,
			Not,
			Error
		};

		struct OnEventResult
		{
			std::string logMsg, chatMsg{}, groupMsg{};
		};

		struct ContextLog
		{
			std::string userId{}, username{}, userTargetId{}, userTargetName{}, userTargetFirstName{}, userTargetLastName{}, chatId{}, title{}, chatType{}, action{};

			static logging::ContextLog ToContextLog(const TgBot::Message::Ptr& message, const std::string& action);
			static logging::ContextLog ToContextLog(const TgBot::ChatMemberUpdated::Ptr& update, const std::string& action);
		};

		logging::LogMode ToLogMode(const std::string_view text) noexcept;

		class Logger final
		{
		public:

			static void Init(const logging::LogMode logMode, const size_t maxFileSize, std::filesystem::path logDirectory);

			static void Deactivation();

			inline static void Log(const logging::LogSource ls, const logging::LogType lt, const std::string_view logText)
			{
				Logger::Get().LogInternal(ls, lt, logText);
			}

			inline static void Log(const logging::LogSource ls, const logging::LogType lt, const logging::ContextLog& contextLog, const std::string_view logText)
			{
				Logger::Get().LogInternal(ls, lt, contextLog, logText);
			}

		private:
			Logger(const Logger&) = delete;
			Logger& operator=(const Logger&) = delete;
			Logger(Logger&&) noexcept = delete;
			Logger& operator=(Logger&&) noexcept = delete;

			Logger() = default;
			~Logger();

			static Logger& Get();

			void OpenNewLogFile();
			void LogFileSwap(const bool newLogFile);

			void LoggingProcess();

			void LogInternal(const logging::LogSource ls, const logging::LogType lt, const std::string_view logText);
			void LogInternal(const logging::LogSource ls, const logging::LogType lt, const logging::ContextLog& contextLog, const std::string_view logText);

			void StopConsole();

			//

			std::vector<std::string> frontBuffer{};
			std::vector<std::string> backBuffer{};

			std::mutex logMutex{};
			std::condition_variable cv{};
			std::thread loggerThread;
			std::atomic<bool> run{ false };

			std::ofstream logFile;
			std::filesystem::path logFilePath{};
			std::filesystem::path logDirectory{};

			size_t currentFileSizeBytes{};
			size_t maxFileSizeBytes{};

			bool toConsole{ false }, toFile{ false };
		};
	}
}