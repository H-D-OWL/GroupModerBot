#pragma once

#include <cstdint>
#include <exception>
#include <random>
#include <string>

#include <tgbot/Bot.h>
#include <tgbot/TgException.h>
#include <tgbot/types/ChatMemberUpdated.h>
#include <tgbot/types/Message.h>

#include <SQLiteCpp/Exception.h>

#include "BotDatabase.h"
#include "Constants.h"
#include "Logging.h"

namespace gmb
{
	class BotController
	{
	public:

		explicit BotController(TgBot::Bot& bot, BotDatabase& botDatabase);
		void Run();

	private:

		logging::OnEventResult OnStart(TgBot::Message::Ptr message);

		logging::OnEventResult OnBotActive(TgBot::Message::Ptr message);
		logging::OnEventResult OnBotDeactive(TgBot::Message::Ptr message);

		logging::OnEventResult OnGroups(TgBot::Message::Ptr message);
		logging::OnEventResult OnSetGroupUniqueTitle(TgBot::Message::Ptr message);

		logging::OnEventResult OnAdmins(TgBot::Message::Ptr message);
		logging::OnEventResult OnAddAdmin(TgBot::Message::Ptr message);
		logging::OnEventResult OnRemoveAdmin(TgBot::Message::Ptr message);

		logging::OnEventResult OnSetWarnMuteSettings(TgBot::Message::Ptr message);
		logging::OnEventResult OnSetWarnBanSettings(TgBot::Message::Ptr message);

		logging::OnEventResult OnSetWarn(TgBot::Message::Ptr message);
		logging::OnEventResult OnViewWarn(TgBot::Message::Ptr message);

		logging::OnEventResult OnMyChatMember(TgBot::ChatMemberUpdated::Ptr update);

		//Provides protection against any exceptions. Logs any exceptions that occur or the correct execution of code.
		template<typename Func>
		void SafeExecute(const logging::ContextLog& contextLog, const Func func) noexcept
		{
			auto SafelySendMessage = [this](const std::string& id, const std::string& textMessage) noexcept
				{
					try
					{
						bot.getApi().sendMessage(id, textMessage);
					}
					catch (...)
					{
						//
					}
				};

			try
			{
				const logging::OnEventResult onEventResult = func();

				if (!onEventResult.logMsg.empty())
					logging::Log(logging::LogSource::Program, logging::LogType::Event, contextLog, onEventResult.logMsg);

				if (!onEventResult.chatMsg.empty())
					SafelySendMessage(contextLog.userId, (contextLog.title.empty() ? "" : contextLog.title + ": ") + onEventResult.chatMsg);

				if (!onEventResult.groupMsg.empty())
					SafelySendMessage(std::string(contextLog.chatId), std::string(onEventResult.groupMsg));
			}
			catch (const SQLite::Exception& e)
			{
				logging::Log(logging::LogSource::Database, logging::LogType::Error, contextLog, e.what());

				SafelySendMessage(contextLog.userId, "Database error: " + std::string{ e.what() });
			}
			catch (const TgBot::TgException& e)
			{
				logging::Log(logging::LogSource::Bot, logging::LogType::Error, contextLog, e.what());

				SafelySendMessage(contextLog.userId, "Telegram error: " + std::string{ e.what() });
			}
			catch (const std::exception& e)
			{
				logging::Log(logging::LogSource::Program, logging::LogType::Error, contextLog, e.what());

				SafelySendMessage(contextLog.userId, "Program error: " + std::string{ e.what() });
			}
			catch (...)
			{
				logging::Log(logging::LogSource::Program, logging::LogType::Error, contextLog, gmb::msg::unknownError);

				SafelySendMessage(contextLog.userId, gmb::msg::unknownError);
			}
		}

		int64_t Fibonacci(const size_t numberOfNumber) const;

		//Generates a string of random characters from 0 to 9 of the specified length.
		std::string RandomNumberGenerator(const size_t length);
		std::random_device rd;
		std::default_random_engine dre{ rd() };
		std::uniform_int_distribution<int> uniform_dist{ '0', '9' };

		std::string confirmationCode{ gmb::consts::invalidTextData }, codeForAddingGroup{ gmb::consts::invalidTextData };

		TgBot::Bot& bot;
		BotDatabase& botDatabase;
	};
}
