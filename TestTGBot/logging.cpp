#include "logging.h"

namespace logging
{
	constexpr const string_view LogPrefixToText(LogPrefix lp)
 {
		switch (lp)
		{
		case LogPrefix::Program:  return "[PROGRAM]";
		case LogPrefix::Database: return "[DATABASE]";
		case LogPrefix::Bot:      return "[BOT]";
		case LogPrefix::Event:    return "[EVENT]";
		case LogPrefix::Error:    return "[ERROR]";
		default:                  return "";
		}
	}

	void Log(const vector<LogPrefix>& logPrefixs, const string_view log = "")
	{
		for (const auto& lp : logPrefixs)
			clog << LogPrefixToText(lp) << ' ';
		clog << log << '\n';
	}
}