#pragma once
#include <string>
#include "BotDatabase.h"
#include "logging.h"
#include <random>
#include <tgbot/tgbot.h>

using namespace std;
using namespace TgBot;
using namespace logging;

class BotController
{
public:

	explicit BotController(BotDatabase& botDatabase, Bot& bot);
	void Run();

private:

	OnEventResult OnStart(Message::Ptr message);
	OnEventResult OnBotActive(Message::Ptr message);
	OnEventResult OnBotDeactive(Message::Ptr message);
	OnEventResult OnGroups(Message::Ptr message);
	OnEventResult OnBan(Message::Ptr message);
	OnEventResult OnUnban(Message::Ptr message);
	OnEventResult OnMute(Message::Ptr message);
	OnEventResult OnUnmute(Message::Ptr message);
	OnEventResult OnNonCommand(Message::Ptr message);
	OnEventResult onMyChatMember(ChatMemberUpdated::Ptr update);
	
	//Provides protection against any exceptions. Logs any exceptions that occur or the correct execution of code.
	template<typename Func>
	void SafeExecute(const ContextLog& contextLog, const Func func) noexcept
	{
		auto SafelySendMessage = [this](const string& userId, const string& textMessage) noexcept
			{
				try
				{
					bot.getApi().sendMessage(userId, textMessage);
				}
				catch (...)
				{

				}
			};

		try
		{
			const OnEventResult onEventResult = func();

			if (!onEventResult.logText.empty())
				Log(LogSource::Program, LogType::Event, contextLog, onEventResult.logText);

			if (!onEventResult.messageText.empty())
				SafelySendMessage(contextLog.userId, onEventResult.messageText.data());
		}
		catch (const SQLite::Exception& e)
		{
			Log(LogSource::Database, LogType::Error, contextLog, e.what());

			SafelySendMessage(contextLog.userId, "Database error: " + string{ e.what() });
		}
		catch (const TgException& e)
		{
			Log(LogSource::Bot, LogType::Error, contextLog, e.what());
			
			SafelySendMessage(contextLog.userId, "Telegram error: " + string{ e.what() });
		}
		catch (const exception& e)
		{
			Log(LogSource::Program, LogType::Error, contextLog, e.what());
			
			SafelySendMessage(contextLog.userId, "Program error: " + string{ e.what() });
		}
		catch (...)
		{
			Log(LogSource::Program, LogType::Error, contextLog, "unknown error");
			
			SafelySendMessage(contextLog.userId, "Unknown error");
		}
	}

	//Checks if the message is a system message.
	bool isSystemMessage(const Message::Ptr& message);

	//Generates a string of random characters from 0 to 9 of the specified length.
	string RandomNumberGenerator(const size_t length);
	random_device rd;
	default_random_engine dre{ rd() };
	uniform_int_distribution<int> uniform_dist{ '0', '9' };

	string confirmationCode{ "ERROR" }, codeForAddingGroup{ "ERROR" };

	Bot& bot; 
	BotDatabase& botDatabase;
};

