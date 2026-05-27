#pragma once

#include <atomic>
#include <cstdint>
#include <exception>
#include <random>
#include <string>

#include <tgbot/Bot.h>
#include <tgbot/TgException.h>
#include <tgbot/types/ChatMemberUpdated.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/LinkPreviewOptions.h>

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

		void Run(const bool enableProcessPendingUpdates = true);

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

		logging::OnEventResult OnDisableBot(TgBot::Message::Ptr message);

		logging::OnEventResult OnMyChatMember(TgBot::ChatMemberUpdated::Ptr update);
		
		//Adds a command to the Telegram UI and a command handler.
		template<typename Func>
		void AddBotCommand(std::string commandName, const Func func) noexcept
		{
			bot.getEvents().onCommand(commandName, [this, commandName, func](TgBot::Message::Ptr message) { if (botWorking) SafeExecute(logging::LogSource::Bot, logging::LogType::Command, logging::ContextLog::ToContextLog(message, commandName), [this, func, message]() { return (this->*func)(message); }); });

			TgBot::BotCommand::Ptr uiCommand(new TgBot::BotCommand);

			uiCommand->description = gmb::consts::command::GetShortDescription(commandName);
			uiCommand->command = std::move(commandName);

			uiCommands.push_back(uiCommand);
		}

		//Provides protection against any exceptions. Logs any exceptions that occur or the correct execution of code.
		template<typename Func>
		void SafeExecute(const logging::LogSource logSource, const logging::LogType logType, const logging::ContextLog& contextLog, const Func func, const bool isLoggingOnly = false) noexcept
		{
			const auto SafelySendMessage = [this, isLoggingOnly](const std::string& id, const std::string& textMessage) noexcept
				{
					if (!isLoggingOnly)
						try
						{
							TgBot::LinkPreviewOptions::Ptr linkPreviewOptions{ std::make_shared<TgBot::LinkPreviewOptions>() };

							linkPreviewOptions->isDisabled = true;

							bot.getApi().sendMessage(id, textMessage, linkPreviewOptions);
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
					logging::Log(logSource, logType, contextLog, onEventResult.logMsg);

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
		
		//Processes "Updates" from Telegram, only those directly related to the bot (kick, ban, adding to a group, changing the bot's status in a group).
		void ProcessPendingUpdates();

		int64_t Fibonacci(size_t numberOfNumber) const;

		//Generates a string of random characters from 0 to 9 of the specified length.
		std::string RandomNumberGenerator(const size_t length);

		//std::uniform_int_distribution<int> uniform_dist{ '0', '9' };

		std::string confirmationCode{ gmb::consts::invalidTextData }, codeForAddingGroup{ gmb::consts::invalidTextData };

		TgBot::Bot& bot;
		BotDatabase& botDatabase;

		std::vector<TgBot::BotCommand::Ptr> uiCommands;

		std::atomic<bool> botWorking = true;
	};
}