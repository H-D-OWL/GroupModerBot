#pragma once

#include <iostream>
#include <vector>
#include <string_view>
#include <tgbot/tgbot.h>

using namespace std;
using namespace TgBot;

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
		Error,
		FatalError,
	};

	struct OnEventResult
	{
		const string_view logText, messageText;
	};

	struct ContextLog
	{
		const string userId{}, username{}, chatId{}, title{}, сommand{};

		static ContextLog ToContextLog(const Message::Ptr& message, const string_view сommand);
		static ContextLog ToContextLog(const ChatMemberUpdated::Ptr& update, const string_view сommand);

	};

	void Log(const LogSource logSource, const LogType logType, const string_view log);
	void Log(const LogSource logSource, const LogType logType, const ContextLog& contextLog, const string_view logText);

}