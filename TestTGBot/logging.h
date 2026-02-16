#pragma once

#include <iostream>
#include <vector>
#include <string_view>

using namespace std;

namespace logging
{
	enum class LogPrefix 
	{
		Program,
		Database,
		Bot,
		Event,
		Error
	};

	void Log(const vector<LogPrefix>& logPrefixs, const string_view log);
}