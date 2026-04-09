#include "logging.h"

//using namespace gmb;
//using namespace logging;

namespace gmb
{
	namespace logging
	{
		static constexpr const std::string_view LogPrefixToText(LogSource ls)
		{
			switch (ls)
			{
			case LogSource::Program:	return "[PROGRAM]";
			case LogSource::Database:	return "[DATABASE]";
			case LogSource::Bot:		return "[BOT]";
			default:					return "";
			}
		}

		static constexpr const std::string_view LogPrefixToText(LogType lt)
		{
			switch (lt)
			{
			case LogType::Event:		return "[EVENT]";
			case LogType::Error:		return "[\033[31mERROR\033[0m]";
			case LogType::FatalError:   return "[\033[31mFATAL ERROR\033[0m]";
			default:					return "";
			}
		}

		static constexpr const std::string_view IsDefined(const std::string_view text)
		{
			return text.empty() ? "undefined" : text;
		}


		void Log(const LogSource logSource, const LogType logType, const std::string_view log)
		{
			std::clog << LogPrefixToText(logSource) << ' ' << LogPrefixToText(logType) << ' ' << log << '\n';
		}

		void Log(const LogSource logSource, const LogType logType, const ContextLog& contextLog, const std::string_view logText)
		{
			std::clog << LogPrefixToText(logSource) << ' ' << LogPrefixToText(logType) << ' '
				<< "[User: " << IsDefined(contextLog.userId) << " (" << IsDefined(contextLog.username) << ") | "
				<< "Chat: " << IsDefined(contextLog.chatId) << " (" << IsDefined(contextLog.title) << ") | "
				<< "Command: " << contextLog.сommand << "] "
				<< logText << '\n';
		}

		ContextLog ContextLog::ToContextLog(const TgBot::Message::Ptr& message, const std::string& сommand)
		{
			const bool isValidUser = message->from != nullptr;
			const bool isValidChat = message->chat != nullptr;

			return ContextLog{
				.userId = isValidUser ? std::to_string(message->from->id) : "!",
				.username = isValidUser ? message->from->username : "",
				.chatId = isValidChat ? std::to_string(message->chat->id) : "",
				.title = isValidChat ? message->chat->title : "",
				.сommand = сommand
			};
		}

		ContextLog ContextLog::ToContextLog(const TgBot::ChatMemberUpdated::Ptr& update, const std::string& сommand)
		{
			const bool isValidUser = update->from != nullptr;
			const bool isValidChat = update->chat != nullptr;

			return ContextLog{
				.userId = isValidUser ? std::to_string(update->from->id) : "",
				.username = isValidUser ? update->from->username : "",
				.chatId = isValidChat ? std::to_string(update->chat->id) : "",
				.title = isValidChat ? update->chat->title : "",
				.сommand = сommand
			};
		}
	}
}