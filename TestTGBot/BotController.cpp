#include "BotController.h"

BotController::BotController(const string& dbPath, const string& botToken, BotDatabase& botDatabase)
{
	botDatabase.Open(dbPath);

	Log({ LogPrefix::Database, LogPrefix::Event }, "batabase: " + dbPath.substr(dbPath.rfind('\\') + 1) + " found");

	botDatabase.CheckStructure();

	botDatabase.CacheReload();

}
