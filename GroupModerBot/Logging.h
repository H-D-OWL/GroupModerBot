#pragma once

#include <string> 
#include <string_view>

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
		 
		struct OnEventResult
		{	
			std::string logMsg, chatMsg{}, groupMsg{};
		};

		struct ContextLog
		{
			std::string userId{}, username{}, userTargetId{}, userTargetname{}, chatId{}, title{}, chatType{}, action{};

			static logging::ContextLog ToContextLog(const TgBot::Message::Ptr& message, const std::string& action);
			static logging::ContextLog ToContextLog(const TgBot::ChatMemberUpdated::Ptr& update, const std::string& action);
		};

		void Log(const logging::LogSource ls, const logging::LogType lt, const std::string_view logText);
		void Log(const logging::LogSource ls, const logging::LogType lt, const logging::ContextLog& contextLog, const std::string_view logText);

		void StopConsole();
	}
}