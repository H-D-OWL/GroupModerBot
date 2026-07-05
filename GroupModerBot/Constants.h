#pragma once

#include <string>
#include <string_view> 

namespace gmb
{
	namespace consts
	{
		inline constexpr std::string_view configFile{ "DataForBot.txt" };
		inline constexpr std::string_view invalidTextData{ "ERROR" };

		namespace msg
		{
			inline const std::string unknownError{ "unknown error" };
			inline const std::string unknownBehavior{ "unknown behavior" };
		}
	}
}