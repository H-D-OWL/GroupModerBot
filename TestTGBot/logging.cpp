#include "logging.h"
//#include <chrono>


namespace logging
{
	constexpr const string_view LogPrefixToText(LogSource ls)
	{
		switch (ls)
		{
		case LogSource::Program:	return "[PROGRAM]";
		case LogSource::Database:	return "[DATABASE]";
		case LogSource::Bot:		return "[BOT]";
		default:					return "";
		}
	}

	constexpr const string_view LogPrefixToText(LogType lt)
	{
		switch (lt)
		{
		case LogType::Event:		return "[EVENT]";
		case LogType::Error:		return "[ERROR]";
		case LogType::FatalError:   return "[FATAL ERROR]";
		default:					return "";
		}
	}



	void Log(const LogSource logSource, const LogType logType, const string_view log)
	{
		clog << LogPrefixToText(logSource) << ' ' << LogPrefixToText(logType) << ' ' << log << '\n';
	}

}