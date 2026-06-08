#include "Logging.h"

#include <iostream>
#include <string> 
#include <string_view>
#include <chrono> 
#include <iomanip>
#include <syncstream>

#include <tgbot/types/ChatMemberUpdated.h> 
#include <tgbot/types/Message.h> 

namespace gmb
{
	namespace logging
	{
		namespace
		{
			std::ostream& logStream{ std::clog };

			constexpr std::string_view LogPrefixToText(const LogSource ls)
			{
				switch (ls)
				{
				case LogSource::Program:	return "[PROGRAM] ";
				case LogSource::Database:	return "[DATABASE]";
				case LogSource::Bot:		return "[BOT]     ";
				default:					return "          ";
				}
			}

			constexpr std::string_view LogPrefixToText(const LogType lt)
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

			std::string ChatTypeToText(const TgBot::Chat::Type type)
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

			void PrintGeneralPartLog(std::ostream& stream, const LogSource ls, const LogType lt)
			{
				const std::chrono::system_clock::time_point now{ std::chrono::system_clock::now() };
				const time_t time{ std::chrono::system_clock::to_time_t(now) };
				tm nowTime{};

#if defined(_MSC_VER) // Windows
				localtime_s(&nowTime, &time);
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__) || defined(__posix) // Linux || macOS || POSIX		
				localtime_r(&time, &nowTime);
#else // Rest
				tm* t{ std::localtime(&time) };

				if (t)
					nowTime = *t;
#endif

				stream << std::put_time(&nowTime, "%Y-%m-%d %H:%M:%S ")
					<< LogPrefixToText(ls) << ' ' << LogPrefixToText(lt) << ' ';
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
				.userTargetname { (hasUserTarget ? message->replyToMessage->from->username : "") },
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

		void Log(const LogSource ls, const LogType lt, const std::string_view logText)
		{
			std::osyncstream syncLogStream(logStream);

			PrintGeneralPartLog(syncLogStream, ls, lt);
			syncLogStream << logText << '\n';
		}

		void Log(const LogSource ls, const LogType lt, const ContextLog& contextLog, const std::string_view logText)
		{
			std::osyncstream syncLogStream(logStream);

			PrintGeneralPartLog(syncLogStream, ls, lt);

			const bool hasUser{ !contextLog.userId.empty() };
			const bool hasUserTarget{ !contextLog.userTargetId.empty() };
			const bool hasChat{ !contextLog.chatId.empty() };
			const bool hasAction{ !contextLog.action.empty() };


			if (hasUser || hasChat || hasAction)
			{
				syncLogStream << '[';

				bool needSeparator{ false };

				if (hasUser)
				{
					syncLogStream << "User: " << contextLog.userId;
					
					if(!contextLog.username.empty()) 
						syncLogStream << " (" << contextLog.username << ')';

					needSeparator = true;
				}

				if (hasUserTarget)
				{
					if (needSeparator)
						syncLogStream << " | ";


					syncLogStream << "Target: " << contextLog.userTargetId;

					if (!contextLog.userTargetname.empty())
						syncLogStream << " (" << contextLog.userTargetname << ')';

					needSeparator = true;
				}

				if (hasChat)
				{
					if (needSeparator)
						syncLogStream << " | ";

					syncLogStream << contextLog.chatType;

					if (contextLog.userId != contextLog.chatId)
					{
						syncLogStream << ": " << contextLog.chatId;

						if (!contextLog.title.empty())
							syncLogStream << " (" << contextLog.title << ')';
					}
				}

				if (hasAction)
				{
					if (needSeparator)
						syncLogStream << " | ";

					syncLogStream << "Action: " << contextLog.action;
				}

				syncLogStream << "] ";
			}

			syncLogStream << logText << '\n';
		}

		void StopConsole()
		{
			std::cin.get();
		}
	}
}
