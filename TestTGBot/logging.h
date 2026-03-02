#pragma once

#include <iostream>
#include <vector>
#include <string_view>

using namespace std;

namespace logging
{
	enum class LogSource
	{
		Program,
		Database,
		Bot,
	};

	enum class LogType
	{
		Event,
		Error,
		FatalError,
	};

	//struct Context
	//{
	//	string memberId{}, username{}, chatId{}, title{}, messageText{}, logText;

	//};

	void Log(const LogSource logSource, const LogType logType, const string_view log);
}