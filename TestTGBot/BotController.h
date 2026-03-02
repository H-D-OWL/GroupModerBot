#pragma once
#include <string>
#include "BotDatabase.h"
#include "logging.h"
#include <random>
#include <tgbot/tgbot.h>
#include <concepts>

using namespace std;
using namespace TgBot;

template<typename T>
concept TelegramData = requires(T a) {
	{ a->from->id } -> std::convertible_to<int64_t>;
	{ a->chat->id } -> std::convertible_to<int64_t>;
};

class BotController
{
public:

	explicit BotController(BotDatabase& botDatabase, Bot& bot);
	void Run();

private:

	void OnStart(Message::Ptr message);
	void OnBotActive(Message::Ptr message);
	void OnBotDeactive(Message::Ptr message);
	void OnGroups(Message::Ptr message);
	void OnBan(Message::Ptr message);
	void OnUnban(Message::Ptr message);
	void OnMute(Message::Ptr message);
	void OnUnmute(Message::Ptr message);
	void OnNonCommand(Message::Ptr message);
	void onMyChatMember(ChatMemberUpdated::Ptr update);

	template<TelegramData T, typename Func>
	void SafeExecute(const T& data, const Func f) 
	{
		string context = "[User: " + to_string(data->from->id) + " (" + (!data->from->username.empty() ? "@" + data->from->username : '\0') + ") | " + "Chat: " + to_string(data->chat->id) + " (" + data->chat->title + ")] ";

		try
		{
			f();
		}
		catch (const SQLite::Exception& e)
		{
			Log(LogSource::Database, LogType::Error, context + e.what());

			try
			{
				bot.getApi().sendMessage(data->from->id, "Произошла ошибка в базой данных: " + string{ e.what() });
			}
			catch (...)
			{

			}
		}
		catch (const TgException& e)
		{
			Log(LogSource::Bot, LogType::Error, context + e.what());

			try
			{
				bot.getApi().sendMessage(data->from->id, "Произошла ошибка с Telegram: " + string{ e.what() });
			}
			catch (...)
			{

			}
		}
		catch (const exception& e)
		{
			Log(LogSource::Program, LogType::Error, context + e.what());

			try
			{
				bot.getApi().sendMessage(data->from->id, "Произошла ошибка: " + string{ e.what() });
			}
			catch (...)
			{

			}
		}
		catch (...)
		{
			Log(LogSource::Program, LogType::Error, "unknown error");

			try
			{
				bot.getApi().sendMessage(data->from->id, "Произошла неизвестная ошибка.");
			}
			catch (...)
			{

			}
		}

	}

	bool isSystemMessage(const Message::Ptr& message);

	string RandomNumberGenerator(const size_t numberOfNumbers);
	random_device rd;
	default_random_engine dre{ rd() };
	uniform_int_distribution<int> uniform_dist{ '0', '9' };

	string confirmationCode{ "ERROR" }, codeForAddingGroup{ "ERROR" };

	Bot& bot; 
	BotDatabase& botDatabase;
};

