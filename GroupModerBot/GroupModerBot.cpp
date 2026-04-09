#define WIN32_LEAN_AND_MEAN

#ifdef _WIN32
#include <windows.h>
#endif

#include <fstream>
#include <string>

#include <tgbot/tgbot.h>
#include <SQLiteCpp/SQLiteCpp.h>

#include "BotController.h"
#include "BotDatabase.h"
#include "logging.h"
#include "Constants.h"


int main()
{
	try
	{
		#ifdef _WIN32
			SetConsoleOutputCP(CP_UTF8);
			SetConsoleCP(CP_UTF8);
		#endif

		std::ifstream fileDataForBot(std::string(gmb::consts::configFileName), std::ios_base::in);

		if (!fileDataForBot.is_open())
			throw std::runtime_error{ []() {
				std::string text = "file \"";
				text += gmb::consts::configFileName;
				text += "\" not found";
				return text;
				}() };

		gmb::logging::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::Event, []() {
			std::string text = "file \"";
			text += gmb::consts::configFileName;
			text += "\" found";
			return text;
			}());

		std::string dbPath{ gmb::consts::invalidTextData }, botToken{ gmb::consts::invalidTextData };

		while (fileDataForBot.good())
		{
			std::string fileLine{};
			getline(fileDataForBot, fileLine);

			if (const size_t offDbPath = fileLine.find(gmb::consts::dbPathKey); offDbPath != std::string::npos)
			{
				dbPath = fileLine.substr(offDbPath + gmb::consts::dbPathKey.size());
			}
			else if (const size_t offBotToken = fileLine.find(gmb::consts::botTokenKey); offBotToken != std::string::npos)
			{
				botToken = fileLine.substr(offBotToken + gmb::consts::botTokenKey.size());
			}
		}

		fileDataForBot.close();

		///

		gmb::BotDatabase botDatabase;

		const std::string& dbTitle = dbPath.substr(dbPath.rfind('\\') + 1);

		botDatabase.Open(dbPath);
		gmb::logging::Log(gmb::logging::LogSource::Database, gmb::logging::LogType::Event, [&dbTitle]() {
			std::string text = "database: \"";
			text += dbTitle;
			text += "\" found";
			return text;
			}());

		botDatabase.CheckStructure();
		gmb::logging::Log(gmb::logging::LogSource::Database, gmb::logging::LogType::Event, [&dbTitle]() {
			std::string text = "database: \"";
			text += dbTitle;
			text += "\" has necessary structure";
			return text;
			}());

		botDatabase.CacheLoad();
		gmb::logging::Log(gmb::logging::LogSource::Database, gmb::logging::LogType::Event, [&dbTitle]() {
			std::string text = "data from database: \"";
			text += dbTitle;
			text += "\" has been loaded into cache";
			return text;
			}());

		///

		//TODO Debug check.
		TgBot::Bot bot2("8231301649:AAEtgMiY1ukuwycs5RWus5IDVfQbrHv7BKo");

		try
		{
			bot2.getApi().sendMessage(-1003528493878, "test text");
		}
		catch (const TgBot::TgException& e)
		{
			gmb::logging::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::FatalError, e.what());
		}
		//-1003528493878

		TgBot::Bot bot(botToken);

		if (bot.getToken().empty())
			throw std::runtime_error{ "botToken is invalid" };

		gmb::BotController botController{ bot, botDatabase };

		gmb::logging::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::Event, [&bot]() {
			std::string text = "bot: \"";
			text += bot.getApi().getMe()->username;
			text += "\" has been launched";
			return text;
			}());

		botController.Run();
	}
	catch (const SQLite::Exception& e)
	{
		gmb::logging::Log(gmb::logging::LogSource::Database, gmb::logging::LogType::FatalError, e.what());
	}
	catch (const TgBot::TgException& e)
	{
		gmb::logging::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::FatalError, e.what());
	}
	catch (const std::exception& e)
	{
		gmb::logging::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::FatalError, e.what());
	}
	catch (...)
	{
		gmb::logging::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::FatalError, gmb::msg::unknownError);
	}

	return 0;
}


