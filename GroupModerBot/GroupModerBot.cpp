#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <fstream>
#include <string>

#include <tgbot/tgbot.h>
#include <SQLiteCpp/SQLiteCpp.h>

#include "BotController.h"
#include "BotDatabase.h"
#include "logging.h"

#include <future>
#include <thread>

/*
* 	1. Obtaining a token and database path from the DataForBot.txt file.
* 	2. Verifying the database path and the presence of all necessary tables and columns, and verifying the token.
* 	3. Issuing a confirmation code to confirm the bot's first administrator rights.
* 	4. Configuring the bot via a private Telegram group.
* 	5. Using group commands (ban, mute, disable chat, etc.).
database

*/

using namespace std;
using namespace TgBot;
using namespace logging;

int main()
{
	try
	{
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);

		ifstream fileDataForBot("DataForBot.txt", ios_base::in);

		if (!fileDataForBot.is_open())
			throw runtime_error{ "file \"DataForBot.txt\" not found" };

		Log(LogSource::Program, LogType::Event, "file \"DataForBot.txt\" found");

		string dbPath{ "ERROR" }, botToken{ "ERROR" };

		while (fileDataForBot.good())
		{
			string fileLine{};
			getline(fileDataForBot, fileLine);

			if (const auto offDbPath = fileLine.find("DbPath="); offDbPath != string::npos)
			{
				dbPath = fileLine.substr(offDbPath + 7);
			}
			else if (const auto offBotToken = fileLine.find("BotToken="); offBotToken != string::npos)
			{
				botToken = fileLine.substr(offBotToken + 9);
			}
		}

		fileDataForBot.close();

		///

		BotDatabase botDatabase;

		botDatabase.Open(dbPath);
		Log(LogSource::Database, LogType::Event, "database: \"" + dbPath.substr(dbPath.rfind('\\') + 1) + "\" found");

		botDatabase.CheckStructure();
		Log(LogSource::Database, LogType::Event, "database: \"" + dbPath.substr(dbPath.rfind('\\') + 1) + "\" has necessary structure");

		botDatabase.CacheLoad();
		Log(LogSource::Database, LogType::Event, "data from database: \"" + dbPath.substr(dbPath.rfind('\\') + 1) + "\" has been loaded into cache");

		///

		//TODO Debug check.
		Bot bot2("8231301649:AAEtgMiY1ukuwycs5RWus5IDVfQbrHv7BKo");
		
		try
		{
			bot2.getApi().sendMessage(-1003528493878, "test text");
		}
		catch (const TgException& e)
		{
			Log(LogSource::Bot, LogType::FatalError, e.what());
		}
		//-1003528493878

		Bot bot(botToken);

		if (bot.getToken().empty())
			throw runtime_error{ "botToken is invalid" };

		BotController botController{ bot, botDatabase };

		Log(LogSource::Bot, LogType::Event, "bot: \"" + bot.getApi().getMe()->username + "\" has been launched");

		botController.Run();
	}
	catch (const SQLite::Exception& e)
	{
		Log(LogSource::Database, LogType::FatalError, e.what());
	}
	catch (const TgException& e)
	{
		Log(LogSource::Bot, LogType::FatalError, e.what());
	}
	catch (const exception& e)
	{
		Log(LogSource::Program, LogType::FatalError, e.what());
	}
	catch (...)
	{
		Log(LogSource::Program, LogType::FatalError, "unknown error");
	}

	return 0;
}


