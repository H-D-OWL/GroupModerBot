#pragma once

#include <atomic>
#include <cstdint> 
#include <exception>
#include <memory>
#include <string>
#include <string_view> 
#include <thread>
#include <vector>
#include <shared_mutex>

#ifdef _WIN32
#include <windows.h>
#include <consoleapi.h>
#endif

#include <tgbot/Bot.h>
#include <tgbot/TgException.h>
#include <tgbot/types/ChatMemberUpdated.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/LinkPreviewOptions.h>
#include <tgbot/types/BotCommand.h>

#include <SQLiteCpp/Exception.h>

#include <taskflow/taskflow.hpp>

#include "BotDatabase.h"
#include "Constants.h"
#include "Logging.h"

namespace gmb
{
	class BotController final
	{
	public:

		BotController(const BotController&) = delete;
		BotController& operator=(const BotController&) = delete;
		BotController(BotController&&) noexcept = delete;
		BotController& operator=(BotController&&) noexcept = delete;

		explicit BotController(TgBot::Bot& bot, BotDatabase& botDatabase);

		~BotController();

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
			bot.getEvents().onCommand(commandName,
				[this, commandName, func](TgBot::Message::Ptr message)
				{
					if (!botWorking) return;

					executor.silent_async([this, commandName, func, message]()
						{
							if (!botWorking) return;

							SafeExecute(logging::LogSource::Bot, logging::LogType::Command, logging::ContextLog::ToContextLog(message, commandName),
								[this, func, message]()
								{
									if (const auto admin{ this->botDatabase.GetAdmin(message->from->id) }; admin)
									{
										const auto& from{ message->from };

										if (admin->firstName != from->firstName
											|| admin->lastName != from->lastName
											|| admin->username != from->username
											|| admin->isBot != from->isBot
											|| admin->isPremium != from->isPremium)
										{
											this->botDatabase.UpdateAdmin(BotDatabase::Admin{
												admin->id,
												from->firstName,
												from->lastName,
												from->username,
												from->isBot,
												from->isPremium,
												admin->isBotOwner });
										}
									}

									return (this->*func)(message);
								}
							);
						}
					);
				}
			);

			TgBot::BotCommand::Ptr uiCommand(new TgBot::BotCommand);
			uiCommand->description = GetShortDescription(commandName);
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
					logging::Logger::Log(logSource, logType, contextLog, onEventResult.logMsg);

				if (!onEventResult.chatMsg.empty())
					SafelySendMessage(contextLog.userId, ChatMessage(contextLog, onEventResult.chatMsg));

				if (!onEventResult.groupMsg.empty())
					SafelySendMessage(std::string(contextLog.chatId), std::string(onEventResult.groupMsg));
			}
			catch (const SQLite::Exception& e)
			{
				logging::Logger::Log(logging::LogSource::Database, logging::LogType::Error, contextLog, e.what());


				SafelySendMessage(contextLog.userId, "Database error: " + std::string{ e.what() });
			}
			catch (const TgBot::TgException& e)
			{
				logging::Logger::Log(logging::LogSource::Bot, logging::LogType::Error, contextLog, e.what());

				SafelySendMessage(contextLog.userId, "Telegram error: " + std::string{ e.what() });
			}
			catch (const std::exception& e)
			{
				logging::Logger::Log(logging::LogSource::Program, logging::LogType::Error, contextLog, e.what());

				SafelySendMessage(contextLog.userId, "Program error: " + std::string{ e.what() });
			}
			catch (...)
			{
				logging::Logger::Log(logging::LogSource::Program, logging::LogType::Error, contextLog, gmb::consts::msg::unknownError);

				SafelySendMessage(contextLog.userId, gmb::consts::msg::unknownError);
			}
		}

		inline std::string GetShortDescription(const std::string_view command);

		std::string ChatMessage(const logging::ContextLog& contextLog, const std::string& chatMsg);

		//Processes "Updates" from Telegram, only those directly related to the bot (kick, ban, adding to a group, changing the bot's status in a group).
		void ProcessPendingUpdates();

#ifdef _WIN32
		static BOOL WINAPI AppCloseHandler(DWORD ctrlType);
#else
		static void AppCloseHandler(int signum);
#endif

		int64_t Fibonacci(size_t sequenceNumber) const noexcept;

		//Generates a std::string of random characters from 0 to 9 of the specified length.
		std::string RandomNumberGenerator(const size_t length);

		std::string confirmationCode{ gmb::consts::invalidTextData };

		TgBot::Bot& bot;
		gmb::BotDatabase& botDatabase;

		tf::Executor executor{ std::thread::hardware_concurrency() };

		std::vector<TgBot::BotCommand::Ptr> uiCommands;

		std::atomic<bool> botWorking = true;

		std::shared_mutex mutexBotController;
	};
}