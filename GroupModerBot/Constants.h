#pragma once

#include <string>

namespace gmb
{
	namespace consts
	{
		inline const std::string configFileName = "DataForBot.txt";
		inline const std::string dbPathKey = "DbPath=";
		inline const std::string botTokenKey = "BotToken=";
		inline const std::string invalidTextData = "ERROR";
	}

	namespace msg
	{
		inline const std::string unknownError = "unknown error";
		inline const std::string unknownBehavior = "unknown behavior";

		namespace log
		{
			inline const std::string privateChat = "call in private chat";
			inline const std::string nonPrivateChat = "call in non-private chat";
			inline const std::string fromOwner = "call fromm owner";
			inline const std::string fromAdmin = "call fromm admin";
			inline const std::string fromPossibleOwner = "call from possible owner";
			inline const std::string fromGuest = "call fromm guest";
			inline const std::string notFromOwner = "call not from owner";
			inline const std::string invalidCommandParameters = "invalid command parameters";
			inline const std::string notReplyToMessage = "message is not reply to message";
			inline const std::string userNotInGroup = "user command is directed to is not in group";
			inline const std::string botIsNotActive = "bot is inactive";
		}

		namespace chat
		{
			inline const std::string cannotUseCommand = "you cannot use this command";
			inline const std::string invalidCommandParameters = "you entered the command parameters incorrectly";
			inline const std::string notReplyToMessage = "ssage is not a reply to the message";
			inline const std::string userNotInGroup = "the user, the command is directed to is not in the group";
			inline const std::string botIsNotActive = "the bot is inactive";
		}

		inline std::string GroupWithUniqueTitleNotFound(const std::string_view uniqueTitle)
		{
			std::string text = "group with uniqueTitle = \"";
			text += uniqueTitle;
			text += "\" not found";

			return text;
		}
	}
}