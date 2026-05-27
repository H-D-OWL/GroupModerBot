#pragma once

#include <string>
#include <unordered_map>

namespace gmb
{
	namespace consts
	{
		inline const std::string configFile{ "DataForBot.txt" };
		inline const std::string standardDBFile{ "GroupModerBotDatabase.db" };
		inline const std::string dbPathKey{ "DbPath=" };
		inline const std::string botTokenKey{ "BotToken=" };
		inline const std::string enableProcessPendingUpdatesKey{ "EnableProcessPendingUpdates=" };

		inline const std::unordered_map<std::string, bool> valuesEnableProcessPendingUpdatesKey{
			{"1", true},
			{"t", true},
			{"true", true},
			{"0", false},
			{"f", false},
			{"false", false},
		};

		inline const std::string invalidTextData{ "ERROR" };

		inline const int64_t defaultNumWarnToMute{ 3 };
		inline const int64_t defaultNumWarnToBan{ 5 };
		inline const size_t numberAttemptsExecuteProcessPendingUpdates{ 3 };

		namespace command
		{
			inline const std::string start = "start";
			inline const std::string botActive = "bot_active";
			inline const std::string botDeactive = "bot_deactive";
			inline const std::string groups = "groups";
			inline const std::string setGroupUniqueTitle = "set_group_unique_title";
			inline const std::string admins = "admins";
			inline const std::string addAdmin = "add_admin";
			inline const std::string removeAdmin = "remove_admin";
			inline const std::string setWarnMuteSettings = "set_warn_mute_settings";
			inline const std::string setWarnBanSettings = "set_warn_ban_settings";
			inline const std::string addWarn = "add_warn";
			inline const std::string removeWarn = "remove_warn";
			inline const std::string setWarn = "set_warn";
			inline const std::string viewWarn = "view_warn";
			inline const std::string disableBot = "disable_bot";

			inline std::string GetShortDescription(const std::string& command)
			{
				static const std::unordered_map<std::string, std::string> shortDescription{
					{start, "Show available commands"},
					{botActive, "Activates the bot. The bot begins executing commands in the group"},
					{botDeactive, "Deactivate the bot. The bot stops executing commands in the group"},
					{groups, "List all groups containing the bot"},
					{setGroupUniqueTitle, "Change the uniqueTitle for a group"},
					{admins, "List all bot administrators"},
					{addAdmin, "Owner: Generate an AdminConfirmationCode. Guest: Become the owner (if none exists) or an admin by entering the confirmation code"},
					{removeAdmin, "Remove an admin using their index number from /admins"},
					{setWarnMuteSettings, "Set the number of warnings after which a group member will be muted. Default: 3. Mute duration (days) = Fibonacci(UserWarns?QuantityWarnToMute)"},
					{setWarnBanSettings, "Set the number of warnings before banning a group member. Default: 5"},
					{addWarn, "Add the specified number of warnings to a member. Default: 1"},
					{removeWarn, "Remove the specified number of warnings to a member. Default: 1"},
					{setWarn, "Set the specified number of warnings for a member"},
					{viewWarn, "Check the current number of warnings for a member"},
					{disableBot, "Turn off the bot completely"}
				};

				if (const auto it = shortDescription.find(command); it != shortDescription.cend())
					return it->second;
				else
				{
					assert(false && "Unknown command");
					return {};
				}
			}
		}

		namespace event
		{
			inline const std::string botChatMemberUpdated{ "bot_chat_member_updated" };
		}
	}

	namespace msg
	{
		inline const std::string unknownError{ "unknown error" };
		inline const std::string unknownBehavior{ "unknown behavior" };

		namespace log
		{
			inline const std::string privateChat = "call in private chat";
			inline const std::string nonPrivateChat = "call in non-private chat";
			inline const std::string fromOwner = "call from owner";
			inline const std::string fromAdmin = "call from admin";
			inline const std::string fromPossibleOwner = "call from possible owner";
			inline const std::string fromGuest = "call from guest";
			inline const std::string notFromOwner = "call not from owner";
			inline const std::string invalidCommandParameters = "invalid command parameters";
			inline const std::string notReplyToMessage = "message is not reply to message";
			inline const std::string userNotInGroup = "user addressed by command is not member of group";
			inline const std::string groupNotFoundInCache = "group not found in cache";
			inline const std::string groupSettingsNotFoundInCache = "group settings not found in cache";
			inline const std::string botIsActive = "bot is active";
			inline const std::string botIsNotActive = "bot is inactive";
		}

		namespace chat
		{
			inline const std::string cannotUseCommand = "you cannot use this command";
			inline const std::string invalidCommandParameters = "you entered the command parameters incorrectly";
			inline const std::string notReplyToMessage = "message is not a reply to the message";
			inline const std::string userNotInGroup = "the user, the command is directed to is not in the group";
			inline const std::string botIsActive = "the bot is active";
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