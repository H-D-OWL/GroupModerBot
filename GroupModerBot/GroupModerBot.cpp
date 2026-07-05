#define WIN32_LEAN_AND_MEAN

#include <exception> 

#ifdef _WIN32
#include <windows.h>
#endif

#include <tgbot/Bot.h>
#include <tgbot/TgException.h>

#include <SQLiteCpp/Exception.h>

#include "ConfigManager.h"
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

		gmb::ConfigManager configManager{ gmb::consts::configFile };

		///

		gmb::logging::Logger::Init(configManager.GetLogMode(), configManager.GetMaxLogFileSize(), configManager.ExtractLogDirectory());

		///

		gmb::BotDatabase botDatabase{};

		botDatabase.Open(configManager.GetDbPath());

		///

		TgBot::Bot bot(configManager.ExtractBotToken());

		gmb::BotController botController{ bot, botDatabase };

		botController.Run(configManager.GetEnableProcessPendingUpdates());
	}
	catch (const SQLite::Exception& e)
	{
		gmb::logging::Logger::Log(gmb::logging::LogSource::Database, gmb::logging::LogType::FatalError, e.what());
	}
	catch (const TgBot::TgException& e)
	{
		gmb::logging::Logger::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::FatalError, e.what());
	}
	catch (const std::exception& e)
	{
		gmb::logging::Logger::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::FatalError, e.what());
	}
	catch (...)
	{
		gmb::logging::Logger::Log(gmb::logging::LogSource::Program, gmb::logging::LogType::FatalError, gmb::consts::msg::unknownError);
	}

	gmb::logging::Logger::Deactivation();

	return 0;
}