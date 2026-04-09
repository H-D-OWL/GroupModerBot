#pragma once

#include <iostream>
#include <string_view>
#include <vector>

#include <tgbot/tgbot.h>

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
			Error,
			FatalError,
		};

		struct OnEventResult
		{	
			const std::string logMsg, chatMsg{}, groupMsg{};
		};

		struct ContextLog
		{
			const std::string userId{}, username{}, chatId{}, title{}, сommand{};

			static logging::ContextLog ToContextLog(const TgBot::Message::Ptr& message, const std::string& сommand);
			static logging::ContextLog ToContextLog(const TgBot::ChatMemberUpdated::Ptr& update, const std::string& сommand);

		};

		void Log(const logging::LogSource logSource, const logging::LogType logType, const std::string_view log);
		void Log(const logging::LogSource logSource, const logging::LogType logType, const logging::ContextLog& contextLog, const std::string_view logText);

	}
}