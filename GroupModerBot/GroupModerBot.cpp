#define WIN32_LEAN_AND_MEAN

#ifdef _WIN32
#include <windows.h>
#include <consoleapi2.h>
#include <WinNls.h>
#endif

#include <filesystem>
#include <fstream>
#include <ios> 
#include <string>
#include <stdexcept>
#include <exception>
#include <algorithm>

#include <tgbot/Bot.h>
#include <tgbot/TgException.h>

#include <SQLiteCpp/Exception.h>

#include "BotController.h"
#include "BotDatabase.h"
#include "Logging.h"
#include "Constants.h"


int main()
{
	try
	{
#ifdef _WIN32
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);
#endif

		std::ifstream fileDataForBot(std::string(gmb::consts::configFile), std::ios_base::in);

		if (!fileDataForBot.is_open())
			throw std::runtime_error{ []() {
				std::string text = "file \"";
				text += gmb::consts::configFile;
				text += "\" not found";
				return text;
				}() };
		
		gmb::logging::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::Event, []() {
			std::string text = "file \"";
			text += gmb::consts::configFile;
			text += "\" found";
			return text;
			}());
		
		//
		
		std::filesystem::path dbPath{};

		std::string botToken{ gmb::consts::invalidTextData };

		bool enableProcessPendingUpdates = true;
		
		//

		std::string fileLine{};

		while (std::getline(fileDataForBot, fileLine))
		{
			fileLine.erase(std::remove(fileLine.begin(), fileLine.end(), '\r'), fileLine.end());

			if (const size_t offDbPath = fileLine.find(gmb::consts::dbPathKey); offDbPath != std::string::npos)
			{
				dbPath = fileLine.substr(offDbPath + gmb::consts::dbPathKey.size());
				continue;
			}
			 
			if (const size_t offBotToken = fileLine.find(gmb::consts::botTokenKey); offBotToken != std::string::npos)
			{
				botToken = fileLine.substr(offBotToken + gmb::consts::botTokenKey.size());
				continue;
			}
			 
			if (const size_t offProcessPendingUpdatesToken = fileLine.find(gmb::consts::enableProcessPendingUpdatesKey); offProcessPendingUpdatesToken != std::string::npos)
			{
				const auto it = gmb::consts::valuesEnableProcessPendingUpdatesKey.find(fileLine.substr(offProcessPendingUpdatesToken + gmb::consts::enableProcessPendingUpdatesKey.size()));

				if(it == gmb::consts::valuesEnableProcessPendingUpdatesKey.cend()) 
					throw std::runtime_error{ "value EnableProcessPendingUpdatesKey is invalid" };

				enableProcessPendingUpdates = it->second;
				continue;
			}
		}

		fileDataForBot.close();

		///

		const std::string dbTitle = [&dbPath]()
			{
				if (dbPath.empty())
				{
					const bool StandardDBExists = std::filesystem::exists(gmb::consts::standardDBFile);

					dbPath = std::filesystem::absolute(gmb::BotDatabase::InitStandardDB()).string();
					
					if(!StandardDBExists)
						gmb::logging::Log(gmb::logging::LogSource::Database, gmb::logging::LogType::Event, []() {
							std::string text = "database: \"";
							text += gmb::consts::standardDBFile;
							text += "\" was created and initialized";
							return text;
							}());
				}

				return dbPath.filename().string();
			}();

		gmb::BotDatabase botDatabase;

		botDatabase.Open(dbPath.string());
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

		TgBot::Bot bot(botToken);

		if (bot.getToken().empty())
			throw std::runtime_error{ "botToken is invalid" };

		gmb::BotController botController{ bot, botDatabase };

		botController.Run(enableProcessPendingUpdates);
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

	gmb::logging::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::Event, "Bot has stopped working. Press any key to close console");

	gmb::logging::StopConsole();

	return 0;
}


