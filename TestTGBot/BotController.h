#pragma once
#include <string>
#include "BotDatabase.h"
#include "logging.h"

using namespace std;

class BotController
{
public:

	BotController(const string& dbPath, const string& botToken, BotDatabase& botDatabase);

private:

};

