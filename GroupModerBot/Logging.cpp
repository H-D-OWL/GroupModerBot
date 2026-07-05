#include "Logging.h"

#include <cassert>  
#include <chrono> 
#include <condition_variable> 
#include <cstdio>  
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <iostream>
#include <mutex> 
#include <shared_mutex> 
#include <sstream>
#include <string> 
#include <string_view> 
#include <system_error>
#include <thread>
#include <utility> 
#include <format>

#include <tgbot/types/ChatMemberUpdated.h> 
#include <tgbot/types/Message.h> 
#include <tgbot/types/Chat.h>

namespace gmb
{
	namespace logging
	{
		namespace
		{
			std::ostream& logStream{ std::clog };

			constexpr std::string_view LogPrefixToText(const LogSource ls) noexcept
			{
				switch (ls)
				{
				case LogSource::Program:	return "[PROGRAM] ";
				case LogSource::Database:	return "[DATABASE]";
				case LogSource::Bot:		return "[BOT]     ";
				default:					return "          ";
				}
			}

			constexpr std::string_view LogPrefixToText(const LogType lt) noexcept
			{
				switch (lt)
				{
				case LogType::Event:		return "[EVENT]      ";
				case LogType::Command:		return "[COMMAND]    ";
				case LogType::Error:		return "[ERROR]      ";
				case LogType::FatalError:   return "[FATAL ERROR]";
				default:					return "             ";
				}
			}

			std::string ChatTypeToText(const TgBot::Chat::Type type) noexcept
			{
				switch (type)
				{
				case TgBot::Chat::Type::Private:	return "Private Chat";
				case TgBot::Chat::Type::Group:		return "Group";
				case TgBot::Chat::Type::Supergroup:	return "Supergroup";
				case TgBot::Chat::Type::Channel:	return "Channel";
				default:							return "";
				}
			}

			inline std::string GeneralPartLog(const LogSource ls, const LogType lt)
			{
				return std::format("{:%Y-%m-%d %H:%M:%S} {} {} ", std::chrono::floor<std::chrono::seconds>(std::chrono::utc_clock::now()), LogPrefixToText(ls), LogPrefixToText(lt));
			}

			inline std::string GetTimestamp()
			{
				return std::format("{:%Y-%m-%d_%H-%M-%S}", std::chrono::floor<std::chrono::seconds>(std::chrono::utc_clock::now()));
			}
		}

		ContextLog ContextLog::ToContextLog(const TgBot::Message::Ptr& message, const std::string& action)
		{
			if (!message) return ContextLog{ .action { action } };

			const bool hasUser{ message->from != nullptr };
			const bool hasChat{ message->chat != nullptr };
			const bool hasUserTarget{ message->replyToMessage != nullptr };

			return ContextLog{
				.userId { (hasUser ? std::to_string(message->from->id) : "") },
				.username { (hasUser ? message->from->username : "") },
				.userTargetId { (hasUserTarget ? std::to_string(message->replyToMessage->from->id) : "") },
				.userTargetName { (hasUserTarget ? message->replyToMessage->from->username : "") },
				.userTargetFirstName { (hasUserTarget ? message->replyToMessage->from->firstName : "") },
				.userTargetLastName { (hasUserTarget ? message->replyToMessage->from->lastName : "") },
				.chatId { (hasChat ? std::to_string(message->chat->id) : "") },
				.title { (hasChat ? message->chat->title : "") },
				.chatType { (hasChat ? ChatTypeToText(message->chat->type) : "") },
				.action { action }
			};
		}

		ContextLog ContextLog::ToContextLog(const TgBot::ChatMemberUpdated::Ptr& update, const std::string& action)
		{
			if (!update) return ContextLog{ .action { action } };

			const bool hasUser{ update->from != nullptr };
			const bool hasChat{ update->chat != nullptr };

			return ContextLog{
				.userId { (hasUser ? std::to_string(update->from->id) : "") },
				.username { (hasUser ? update->from->username : "") },
				.chatId { (hasChat ? std::to_string(update->chat->id) : "") },
				.title { (hasChat ? update->chat->title : "") },
				.chatType { (hasChat ? ChatTypeToText(update->chat->type) : "") },
				.action { action }
			};
		}

		LogMode ToLogMode(const std::string_view text) noexcept
		{
			if (text == "Console")
			{
				return LogMode::Console;
			}
			else if (text == "File")
			{
				return LogMode::File;
			}
			else if (text == "ConsoleAndFile")
			{
				return LogMode::ConsoleAndFile;
			}
			else if (text == "Not")
			{
				return LogMode::Not;
			}
			else
			{
				return LogMode::Error;
			}
		}

		///

		void Logger::Init(const LogMode logMode, const size_t maxFileSizeBytes, std::filesystem::path logDirectory)
		{
			Get().toConsole = logMode == LogMode::Console || logMode == LogMode::ConsoleAndFile;
			Get().toFile = logMode == LogMode::File || logMode == LogMode::ConsoleAndFile;

			Get().maxFileSizeBytes = maxFileSizeBytes;

			Get().logDirectory = std::move(logDirectory);

			Get().frontBuffer.reserve(100);
			Get().backBuffer.reserve(100);

			if (Get().toFile)
			{
				std::error_code ec;

				std::filesystem::create_directories(Get().logDirectory, ec);

				if (ec)
				{
					Get().toFile = false;

					Get().LogInternal(LogSource::Program, LogType::Error, "failed to create a log directory. Logging to a file is disabled");
				}
				else
					Get().OpenNewLogFile();
			}

			Get().run = true;

			Get().loggerThread = std::thread([]() { Get().LoggingProcess(); });
		}

		void Logger::Deactivation()
		{
			const bool stopConsole{ Get().run == false || (Get().toConsole && !Get().toFile) };

			if (stopConsole)
				Get().LogInternal(gmb::logging::LogSource::Program, gmb::logging::LogType::Event, "Bot has stopped working. Press ENTER to close console");
			else
				Get().LogInternal(gmb::logging::LogSource::Program, gmb::logging::LogType::Event, "Bot has stopped working");

			Get().run = false;

			Get().cv.notify_one();

			if (Get().loggerThread.joinable())
				Get().loggerThread.join();
			else
			{
				std::scoped_lock lock(Get().logMutex);

				for (const std::string_view log : Get().frontBuffer)
				{
					logStream << log;
				}

				Get().frontBuffer.clear();
			}

			if (Get().toFile && Get().logFile.is_open())
				Get().LogFileSwap(false);

			if (stopConsole)
				Get().StopConsole();
		}

		Logger::~Logger()
		{
			run = false;

			cv.notify_one();

			if (loggerThread.joinable())
				loggerThread.join();
			else
			{
				std::scoped_lock lock(logMutex);

				for (const std::string_view log : frontBuffer)
				{
					logStream << log;
				}
			}

			if (toFile && logFile.is_open()) LogFileSwap(false);
		}

		Logger& Logger::Get()
		{
			static Logger logger{};

			return logger;
		}

		void Logger::OpenNewLogFile()
		{
			logFilePath = logDirectory / std::format("log_{}.txt", GetTimestamp());

			logFile.open(logFilePath, std::ios::app);

			if (!logFile.is_open())
			{
				toFile = false;

				LogInternal(LogSource::Program, LogType::Error, "failed to create a log file. Logging to a file is disabled");
			}

			currentFileSizeBytes = 0;
		}

		void Logger::LogFileSwap(const bool newLogFile = true)
		{
			logFile.close(); 

			std::filesystem::path newLogFileName{ logFilePath };
			newLogFileName.replace_extension();
			newLogFileName += "—";
			newLogFileName += GetTimestamp();
			newLogFileName += ".txt";

			std::error_code ec;

			std::filesystem::rename(logFilePath, newLogFileName, ec);

			if (ec) 
				LogInternal(LogSource::Program, LogType::Error, "failed to rename a log file");

			if (newLogFile) OpenNewLogFile();
		}

		void Logger::LoggingProcess()
		{
			while (run || !frontBuffer.empty())
			{
				{
					std::unique_lock lock(logMutex);

					cv.wait_for(lock, std::chrono::milliseconds(500), [this]() -> bool {
						return !frontBuffer.empty() || !run; });

					frontBuffer.swap(backBuffer);
				}

				if (!backBuffer.empty())
				{
					for (const std::string_view log : backBuffer)
					{
						if (toConsole)
						{
							logStream << log;
						}

						if (toFile)
						{
							logFile << log;

							currentFileSizeBytes += log.size();
						}
					}

					if (toFile) logFile.flush();

					backBuffer.clear();

					if (toFile && currentFileSizeBytes >= maxFileSizeBytes) LogFileSwap();
				}
			}
		}

		void Logger::LogInternal(const LogSource ls, const LogType lt, const std::string_view logText)
		{
			std::string log{ GeneralPartLog(ls, lt) };
			log += logText;
			log += '\n';

			std::scoped_lock lock(logMutex);

			frontBuffer.emplace_back(std::move(log));

			if (frontBuffer.size() >= 1000 || lt == LogType::FatalError || lt == LogType::Error)
				cv.notify_one();
		}

		void Logger::LogInternal(const LogSource ls, const LogType lt, const ContextLog& contextLog, const std::string_view logText)
		{
			std::string log{ GeneralPartLog(ls, lt) };

			const bool hasUser{ !contextLog.userId.empty() };
			const bool hasUserTarget{ !contextLog.userTargetId.empty() };
			const bool hasChat{ !contextLog.chatId.empty() };
			const bool hasAction{ !contextLog.action.empty() };

			if (hasUser || hasUserTarget || hasChat || hasAction)
			{
				log += '[';

				bool needSeparator{ false };

				if (hasUser)
				{
					log += "User: ";
					log += contextLog.userId;

					if (!contextLog.username.empty())
					{
						log += " (";
						log += contextLog.username;
						log += ')';
					}

					needSeparator = true;
				}

				if (hasUserTarget)
				{
					if (needSeparator)
						log += " | ";


					log += "Target: ";
					log += contextLog.userTargetId;

					if (!contextLog.userTargetName.empty())
					{
						log += " (";
						log += contextLog.userTargetName;
						log += ')';
					}

					needSeparator = true;
				}

				if (hasChat)
				{
					if (needSeparator)
						log += " | ";

					log += contextLog.chatType;

					if (contextLog.userId != contextLog.chatId)
					{
						log += ": ";
						log += contextLog.chatId;

						if (!contextLog.title.empty())
						{
							log += " (";
							log += contextLog.title;
							log += ')';
						}
					}
				}

				if (hasAction)
				{
					if (needSeparator)
						log += " | ";

					log += "Action: ";
					log += contextLog.action;
				}

				log += "] ";
			}

			log += logText;
			log += '\n';

			std::scoped_lock lock(logMutex);

			frontBuffer.emplace_back(std::move(log));

			if (frontBuffer.size() >= 1000) cv.notify_one();
		}

		void Logger::StopConsole()
		{
			std::cin.get();
		}
	}
}
