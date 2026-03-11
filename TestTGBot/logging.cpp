#include "logging.h"
//#include <chrono>


namespace logging
{
	constexpr const string_view LogPrefixToText(LogSource ls)
	{
		switch (ls)
		{
		case LogSource::Program:	return "[PROGRAM]";
		case LogSource::Database:	return "[DATABASE]";
		case LogSource::Bot:		return "[BOT]";
		default:					return "";
		}
	}

	constexpr const string_view LogPrefixToText(LogType lt)
	{
		switch (lt)
		{
		case LogType::Event:		return "[EVENT]";
		case LogType::Error:		return "[\033[31mERROR\033[0m]";
		case LogType::FatalError:   return "[\033[31mFATAL ERROR\033[0m]";
		default:					return "";
		}
	}

	constexpr const string_view IsDefined(const string_view text)
	{
		return text.empty() ? "undefined" : text;
	}


	void Log(const LogSource logSource, const LogType logType, const string_view log)
	{
		clog << LogPrefixToText(logSource) << ' ' << LogPrefixToText(logType) << ' ' << log << '\n';
	}

	void Log(const LogSource logSource, const LogType logType, const ContextLog& contextLog, const string_view logText)
	{
		clog	<< LogPrefixToText(logSource)					<< ' '	<< LogPrefixToText(logType)			<< ' ' 
				<< "[User: "	<< IsDefined(contextLog.userId)	<< " (" << IsDefined(contextLog.username)	<< ") | "
				<< "Chat: "		<< IsDefined(contextLog.chatId)	<< " (" << IsDefined(contextLog.title)		<< ") | "
				<< "Command: "	<< contextLog.сommand			<< "] "
				<< logText << '\n';
	}

	ContextLog ContextLog::ToContextLog(const Message::Ptr& message, const string_view сommand)
	{
		const bool isValidUser = message->from != nullptr;
		const bool isValidChat = message->chat != nullptr;

		return ContextLog{
			.userId		= isValidUser ? to_string(message->from->id) : "!",
			.username	= isValidUser ? message->from->username : "",
			.chatId		= isValidChat ? to_string(message->chat->id) : "",
			.title		= isValidChat ? message->chat->title : "",
			.сommand	= сommand.data()
		};
	}

	ContextLog ContextLog::ToContextLog(const ChatMemberUpdated::Ptr& update, const string_view сommand)
	{
		const bool isValidUser = update->from != nullptr;
		const bool isValidChat = update->chat != nullptr;

		return ContextLog{
			.userId		= isValidUser ? to_string(update->from->id) : "",
			.username	= isValidUser ? update->from->username : "",
			.chatId		= isValidChat ? to_string(update->chat->id) : "",
			.title		= isValidChat ? update->chat->title : "",
			.сommand	= сommand.data()
		};
	}
}