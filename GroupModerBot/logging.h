#pragma once

#include <string> 
#include <string_view>

#include <tgbot/types/ChatMemberUpdated.h> 
#include <tgbot/types/Message.h> 

//#include <tgbot/tgbot.h>
//#include <tgbot/types/Message.h>
//#include <tgbot/types/ChatMemberUpdated.h>

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
			const std::string userId{}, username{}, chatId{}, title{}, command{};
			
			static logging::ContextLog ToContextLog(const TgBot::Message::Ptr& message, const std::string& command);
			static logging::ContextLog ToContextLog(const TgBot::ChatMemberUpdated::Ptr& update, const std::string& command);

		};

		void Log(const logging::LogSource logSource, const logging::LogType logType, const std::string_view log);
		void Log(const logging::LogSource logSource, const logging::LogType logType, const logging::ContextLog& contextLog, const std::string_view logText);

	}
}