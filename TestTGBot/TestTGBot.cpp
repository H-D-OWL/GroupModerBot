#include <iostream>
#include <stdio.h>
#include <tgbot/tgbot.h>
#include <string>
#include <vector>
#include <fstream>
#include <SQLiteCpp/SQLiteCpp.h>
#include <memory>
#include <unordered_map>
#include <random>
#include "logging.h"
#include <chrono>
#include "BotDatabase.h"
#include "BotController.h"

/*
* 	1. Obtaining a token and database path from the DataForBot.txt file.
* 	2. Verifying the database path and the presence of all necessary tables and columns, and verifying the token.
* 	3. Issuing a confirmation code to confirm the bot's first administrator rights.
* 	4. Configuring the bot via a private Telegram group.
* 	5. Using group commands (ban, mute, disable chat, etc.).
database

*/

/*
*	1. Create and configure full-fledged data warehouses for groups and users (from March 1, 2026 - 2 days) (completed in 2 days)
*	2. Rename the project and upload it to GitHub (starts March 3 - 1 day)
* 
* 
* 
* 
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
		
		///
		
		ifstream fileDataForBot("DataForBot.txt", ios_base::in);

		if (!fileDataForBot.is_open())
			throw runtime_error{ "file \"DataForBot.txt\" not found" };

		Log(LogSource::Program, LogType::Event, "file \"DataForBot.txt\" found");

		string dbPath{ "ERROR" }, botToken{ "ERROR" };

		while (fileDataForBot.good())
		{
			string fileLine{};
			getline(fileDataForBot, fileLine);

			if (const auto off = fileLine.find("DbPath="); off != string::npos)
				dbPath = fileLine.substr(off + 7);
			else if (const auto off = fileLine.find("BotToken="); off != string::npos)
				botToken = fileLine.substr(off + 9);
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
		
		Bot bot(botToken);

		if (bot.getToken().empty())
			throw runtime_error{ "botToken is invalid" };

		BotController botController{ botDatabase, bot };
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


