#include "BotController.h"

#include <algorithm> 
#include <cctype>    
#include <chrono> 
#include <thread>
#include <corecrt.h>
#include <cstdint> 
#include <sstream> 
#include <string> 

#include <tgbot/Bot.h>
#include <tgbot/net/TgLongPoll.h> 
#include <tgbot/types/Chat.h> 
#include <tgbot/types/ChatMemberUpdated.h>
#include <tgbot/types/ChatPermissions.h> 
#include <tgbot/types/Message.h>

#include "BotDatabase.h"
#include "Constants.h"
#include "Logging.h"

namespace gmb
{
	BotController::BotController(TgBot::Bot& bot, BotDatabase& botDatabase) : bot(bot), botDatabase(botDatabase)
	{
		if (!botDatabase.GetNumberAdmins())
		{
			confirmationCode.clear();

			confirmationCode = RandomNumberGenerator(64);

			logging::Log(logging::LogSource::Program, logging::LogType::Event, "confirmation code: " + confirmationCode);
		}

		bot.getEvents().onCommand("start",					[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "start"),						[&]() { return OnStart(message); }); });

		bot.getEvents().onCommand("botActive",				[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "botActive"),					[&]() { return OnBotActive(message); }); });
		bot.getEvents().onCommand("botDeactive",			[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "botDeactive"),				[&]() { return OnBotDeactive(message); }); });

		bot.getEvents().onCommand("groups",					[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "groups"),						[&]() { return OnGroups(message); }); });
		bot.getEvents().onCommand("setGroupUniqueTitle",	[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "setGroupUniqueTitle"),		[&]() { return OnSetGroupUniqueTitle(message); }); });

		bot.getEvents().onCommand("admins",					[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "admins"),						[&]() { return OnAdmins(message); }); });
		bot.getEvents().onCommand("addAdmin",				[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "addAdmin"),					[&]() { return OnAddAdmin(message); }); });
		bot.getEvents().onCommand("removeAdmin",			[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "removeAdmin"),				[&]() { return OnRemoveAdmin(message); }); });

		bot.getEvents().onCommand("setWarnMuteSettings",	[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "setWarnMuteSettings"),		[&]() { return OnSetWarnMuteSettings(message); }); });
		bot.getEvents().onCommand("setWarnBanSettings",		[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "setWarnBanSettings"),			[&]() { return OnSetWarnBanSettings(message); }); });

		bot.getEvents().onCommand("addWarn",				[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "addWarn"),					[&]() { return OnSetWarn(message); }); });
		bot.getEvents().onCommand("removeWarn",				[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "removeWarn"),					[&]() { return OnSetWarn(message); }); });
		bot.getEvents().onCommand("setWarn",				[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "setWarn"),					[&]() { return OnSetWarn(message); }); });
		bot.getEvents().onCommand("viewWarn",				[this](TgBot::Message::Ptr message) { SafeExecute(logging::ContextLog::ToContextLog(message, "viewWarn"),					[&]() { return OnViewWarn(message); }); });

		bot.getEvents().onMyChatMember(						[this](TgBot::ChatMemberUpdated::Ptr update) { SafeExecute(logging::ContextLog::ToContextLog(update, "changeMyChatMember"),	[&]() { return OnMyChatMember(update); }); });
	}

	void BotController::Run()
	{
		TgBot::TgLongPoll longPoll(bot);

		while (true)
		{
			SafeExecute(logging::ContextLog{}, [&]() -> logging::OnEventResult {
				while (true) { longPoll.start(); }
				return { "", "" }; });

			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
	}

	logging::OnEventResult BotController::OnStart(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (botDatabase.IsOwner(message->from->id))
		{
			return { gmb::msg::log::fromOwner, R"(
I can help you maintain discipline in your Telegram groups. Documentation (https://github.com/H-D-OWL/GroupModerBot).

You can control me by sending the following commands:

In a private chat:

/start - get information about commands

Group Settings
/groups - get information about groups containing the bot
/setGroupUniqueTitle - change the uniqueTitle for a group

Managing Bot Administrators
/admins - get information about all bot administrators
/addAdmin - get the Administrator Verification Code
/removeAdmin - remove a bot administrator

Warning Settings
/setWarnMuteSettings - set the number of warnings before a group member is mute
/setWarnBanSettings - set the number of warnings before banning a group member

In a group:

Bot Management
/botActive - activates the bot in the group
/botDeactive - deactivates the bot in the group

Managing Warnings

/addWarn - add the specified number of warnings to a member
/removeWarn - remove the specified number of warnings to a member member
/setWarn - set the specified number of warnings for a member
/viewWarn - get the number of warnings for a member
)" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { gmb::msg::log::fromAdmin, R"(
I can help you maintain discipline in your Telegram groups. Documentation (https://github.com/H-D-OWL/GroupModerBot).

You can control me by sending the following commands:

In a private chat:

/start - get information about commands

Group Settings
/groups - get information about groups containing the bot

Managing Bot Administrators
/admins - get information about all bot administrators

Warning Settings
/setWarnMuteSettings - set the number of warnings before a group member is mute
/setWarnBanSettings - set the number of warnings before banning a group member

In a group:

Managing Warnings
/addWarn - add the specified number of warnings to a member
/removeWarn - remove the specified number of warnings to a member member
/setWarn - set the specified number of warnings for a member
/viewWarn - get the number of warnings for a member
)" };
		}
		else if (botDatabase.GetNumberAdmins() == 0)
		{
			return { gmb::msg::log::fromPossibleOwner, R"(
I can help you maintain discipline in your Telegram groups. Documentation (https://github.com/H-D-OWL/GroupModerBot).

You can control me by sending the following commands:

In a private chat:

/start - get information about commands

Managing Bot Administrators
/addAdmin - become an owner by entering the Verification Code
)" };
		}
		else
		{
			return { gmb::msg::log::fromGuest, R"(
I can help you maintain discipline in your Telegram groups. Documentation (https://github.com/H-D-OWL/GroupModerBot).

You can control me by sending the following commands:

In a private chat:

/start - get information about commands

Managing Bot Administrators
/addAdmin - become an administrator by entering the Administrator Verification Code
)" };
		}
	}

	logging::OnEventResult BotController::OnBotActive(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { gmb::msg::log::privateChat, "" };

		if (botDatabase.IsOwner(message->from->id))
		{
			const BotDatabase::Group& group = botDatabase.GetGroups().at(message->chat->id);
			const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(group.id);

			BotDatabase::Group updateGroup = group;
			updateGroup.isBotActive = true;

			botDatabase.UpdateGroup(updateGroup, groupSettings);

			return { "bot has been activated", "The bot has been activated" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { gmb::msg::log::fromAdmin, gmb::msg::chat::cannotUseCommand };
		}
		else
		{
			return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };
		}
	}

	logging::OnEventResult BotController::OnBotDeactive(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { gmb::msg::log::privateChat, "" };

		if (!botDatabase.IsBotActive(message->chat->id)) return { gmb::msg::log::botIsNotActive,  gmb::msg::chat::botIsNotActive };

		if (botDatabase.IsOwner(message->from->id))
		{
			const BotDatabase::Group& group = botDatabase.GetGroups().at(message->chat->id);
			const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(group.id);

			BotDatabase::Group updateGroup = group;
			updateGroup.isBotActive = false;

			botDatabase.UpdateGroup(updateGroup, groupSettings);

			return { "bot has been deactivated", "The bot has been deactivated" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { gmb::msg::log::fromAdmin, gmb::msg::chat::cannotUseCommand };
		}
		else
		{
			return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };
		}
	}

	logging::OnEventResult BotController::OnGroups(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		std::string sendMessageText{};

		size_t number{ 1 };

		for (const auto& [id, groupFromCache] : botDatabase.GetGroups())
		{
			sendMessageText += std::to_string(number);
			sendMessageText += ". ";
			sendMessageText += groupFromCache.title;
			sendMessageText += " (";
			sendMessageText += groupFromCache.uniqueTitle;
			sendMessageText += ")";
			sendMessageText += ":\n    IsBotAdmin: ";
			sendMessageText += (groupFromCache.isBotAdmin ? "Yes" : "No");
			sendMessageText += "\n    ";
			sendMessageText += "IsBotActive: ";
			sendMessageText += (groupFromCache.isBotActive ? "Yes" : "No");
			sendMessageText += "\n    ";

			const gmb::BotDatabase::GroupSettings* groupSettingsFromCache = botDatabase.GetGroupSettings(id);

			if (groupSettingsFromCache)
			{
				sendMessageText += "NumWarnToMute: ";
				sendMessageText += std::to_string(groupSettingsFromCache->numWarnToMute);
				sendMessageText += "\n    ";
				sendMessageText += "NumWarnToBan: ";
				sendMessageText += std::to_string(groupSettingsFromCache->numWarnToBan);
				sendMessageText += "\n    ";
			}

			sendMessageText += '\n';
			++number;
		}

		if (number == 1)
		{
			sendMessageText = "There are no groups";
		}

		return { "list of groups has been viewed", sendMessageText };
	}

	logging::OnEventResult BotController::OnSetGroupUniqueTitle(TgBot::Message::Ptr message)
	{
		using namespace std::string_view_literals;

		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsOwner(message->from->id)) return { gmb::msg::log::notFromOwner, gmb::msg::chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr("/setGroupUniqueTitle"sv.size()));

		std::string oldUniqueTitle{}, newUniqueTitle{};

		if (!(commandParameters >> oldUniqueTitle >> newUniqueTitle)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (newUniqueTitle.length() > 32) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (!std::all_of(newUniqueTitle.begin(), newUniqueTitle.end(), [](unsigned char c) { return isalnum(c) || c == '_'; })) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle))) return { gmb::msg::GroupWithUniqueTitleNotFound(oldUniqueTitle), gmb::msg::GroupWithUniqueTitleNotFound(oldUniqueTitle) };

		const BotDatabase::Group& group = botDatabase.GetGroups().at(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle));
		const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(group.id);

		BotDatabase::Group updateGroup = group;
		updateGroup.uniqueTitle = newUniqueTitle;

		botDatabase.UpdateGroup(updateGroup, groupSettings);

		return { "unique title changed from " + oldUniqueTitle + " to " + newUniqueTitle, "unique title changed from " + oldUniqueTitle + " to " + newUniqueTitle };
	}

	logging::OnEventResult BotController::OnAdmins(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		std::string sendMessageText{};

		size_t number{ 1 };

		for (const auto& [id, adminFromCache] : botDatabase.GetAdmins())
		{
			sendMessageText += std::to_string(number);
			sendMessageText += ". ";
			sendMessageText += adminFromCache.username
				+ " ("
				+ adminFromCache.firstName
				+ ' '
				+ adminFromCache.lastName + ")"
				+ ":\n    IsBotOwner: "
				+ (adminFromCache.isBotOwner ? "Yes" : "No")
				+ "\n    "
				+ "IsBot: "
				+ (adminFromCache.isBot ? "Yes" : "No")
				+ "\n    "
				+ "IsPremium: "
				+ (adminFromCache.isPremium ? "Yes" : "No");
			sendMessageText += '\n';
			++number;
		}

		if (number == 1)
		{
			sendMessageText = "There are no admins";
		}

		return { "list of admins has been viewed", sendMessageText};
	}

	logging::OnEventResult BotController::OnAddAdmin(TgBot::Message::Ptr message)
	{
		using namespace std::string_view_literals;

		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (botDatabase.IsOwner(message->from->id))
		{
			confirmationCode.clear();

			confirmationCode = RandomNumberGenerator(32);

			return { "confirmation code generated", "confirmation code = " + confirmationCode };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { gmb::msg::log::fromAdmin, gmb::msg::chat::cannotUseCommand };
		}
		else
		{
			std::stringstream commandParameters(message->text.substr("/addAdmin"sv.size()));

			std::string adminConfirmationCode{};

			if (!(commandParameters >> adminConfirmationCode)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

			if (confirmationCode != "ERROR" && adminConfirmationCode == confirmationCode && botDatabase.GetNumberAdmins() == 0)
			{
				botDatabase.AddAdmin(BotDatabase::Admin{
				message->from->id,
				message->from->firstName,
				message->from->lastName,
				message->from->username,
				message->from->isBot,
				message->from->isPremium,
				true
					});

				confirmationCode = "ERROR";

				return { "user became bot owner", "you have become a bot owner" };
			}
			else if (confirmationCode != "ERROR" && adminConfirmationCode == confirmationCode && botDatabase.GetNumberAdmins() > 0)
			{
				botDatabase.AddAdmin(BotDatabase::Admin{
				message->from->id,
				message->from->firstName,
				message->from->lastName,
				message->from->username,
				message->from->isBot,
				message->from->isPremium,
				false
					});

				confirmationCode = "ERROR";

				return { "user became bot admin", "you have become a bot admin" };
			}
			else if (confirmationCode != "ERROR" && !adminConfirmationCode.empty())
			{
				return { "confirmation code is incorrect", "The confirmation code is incorrect" };
			}
			else if (confirmationCode == "ERROR")
			{
				return { "confirmation code not generated", "confirmation code not generated" };
			}

			return { gmb::msg::unknownError, gmb::msg::unknownError };
		}
	}

	logging::OnEventResult BotController::OnRemoveAdmin(TgBot::Message::Ptr message)
	{
		using namespace std::string_view_literals;

		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsOwner(message->from->id)) return { gmb::msg::log::notFromOwner, gmb::msg::chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr("/removeAdmin"sv.size()));

		size_t adminNumber{};

		if (!(commandParameters >> adminNumber)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		--adminNumber;

		if (!(adminNumber >= 0 && adminNumber < botDatabase.GetNumberAdmins())) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		size_t count = 0;

		for (const auto& [id, adminFromCache] : botDatabase.GetAdmins())
		{
			if (count == adminNumber)
			{
				if (!botDatabase.IsOwner(id))
				{
					botDatabase.DeleteAdmin(id);
					break;
				}
				else
				{
					return { "attempt to remove bot owner", "the bot owner cannot be removed" };
				}
			}

			++count;
		}

		return { "admin has been removed", "the admin has been removed" };
	}

	logging::OnEventResult BotController::OnSetWarnMuteSettings(TgBot::Message::Ptr message)
	{
		using namespace std::string_view_literals;

		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr("/setWarnMuteSettings"sv.size()));

		std::string uniqueTitle{};

		int64_t numWarnToMute{};

		if (!(commandParameters >> uniqueTitle >> numWarnToMute)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (numWarnToMute < 0) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(uniqueTitle))) return { gmb::msg::GroupWithUniqueTitleNotFound(uniqueTitle), gmb::msg::GroupWithUniqueTitleNotFound(uniqueTitle) };

		const BotDatabase::Group& group = botDatabase.GetGroups().at(botDatabase.GroupIdFromUniqueTitle(uniqueTitle));
		const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(group.id);

		BotDatabase::GroupSettings updategroupSettings = groupSettings;
		updategroupSettings.numWarnToMute = numWarnToMute;

		botDatabase.UpdateGroup(group, updategroupSettings);

		return { "warn mute settings changed", "warn mute settings changed" };
	}

	logging::OnEventResult BotController::OnSetWarnBanSettings(TgBot::Message::Ptr message)
	{
		using namespace std::string_view_literals;

		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr("/setWarnBanSettings"sv.size()));

		std::string uniqueTitle{};

		int64_t numWarnToBan{};

		if (!(commandParameters >> uniqueTitle >> numWarnToBan)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (numWarnToBan < 0) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(uniqueTitle))) return { gmb::msg::GroupWithUniqueTitleNotFound(uniqueTitle), gmb::msg::GroupWithUniqueTitleNotFound(uniqueTitle) };

		const BotDatabase::Group& group = botDatabase.GetGroups().at(botDatabase.GroupIdFromUniqueTitle(uniqueTitle));
		const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(group.id);

		BotDatabase::GroupSettings updategroupSettings = groupSettings;
		updategroupSettings.numWarnToBan = numWarnToBan;

		botDatabase.UpdateGroup(group, updategroupSettings);

		return { "warn ban settings changed", "warn ban settings changed" };
	}

	logging::OnEventResult BotController::OnSetWarn(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { gmb::msg::log::privateChat, "" };

		if (!botDatabase.IsBotActive(message->chat->id)) return { gmb::msg::log::botIsNotActive,  gmb::msg::chat::botIsNotActive };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		const TgBot::Message::Ptr replyToMessage = message->replyToMessage;

		if (!replyToMessage) return { gmb::msg::log::notReplyToMessage,  gmb::msg::chat::notReplyToMessage };

		if (botDatabase.IsAdmin(replyToMessage->from->id)) return { "admins cannot influence each other", "admins cannot influence each other" };

		std::stringstream commandParameters(message->text);

		std::string comand{};

		if (!(commandParameters >> comand)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(message->chat->id);

		const std::string memberStatus = bot.getApi().getChatMember(groupSettings.id, replyToMessage->from->id)->status;

		if (memberStatus == "left") return { gmb::msg::log::userNotInGroup, gmb::msg::chat::userNotInGroup };

		const int64_t previousQuantityWarn = botDatabase.GetWarns(replyToMessage->from->id, groupSettings.id);

		int64_t newQuantityWarns = 0, arg = 0;

		if (comand == "/addWarn")
		{
			if (!(commandParameters >> arg)) arg = 1;

			newQuantityWarns = previousQuantityWarn + arg;
		}
		else if (comand == "/removeWarn")
		{
			if (!(commandParameters >> arg)) arg = 1;

			newQuantityWarns = previousQuantityWarn - arg;
		}
		else if (comand == "/setWarn")
		{
			if (!(commandParameters >> newQuantityWarns)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };
		}
		else
			return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		newQuantityWarns = std::max(0ll, newQuantityWarns);

		if (newQuantityWarns == 0)
			botDatabase.DeleteWarns(replyToMessage->from->id, groupSettings.id);
		else
			botDatabase.SetWarns(replyToMessage->from->id, groupSettings.id, newQuantityWarns);

		if (newQuantityWarns >= groupSettings.numWarnToBan && groupSettings.numWarnToBan > 0)
		{
			if (memberStatus == "kicked") return { gmb::msg::log::userNotInGroup, gmb::msg::chat::userNotInGroup };

			if (!bot.getApi().banChatMember(replyToMessage->chat->id, replyToMessage->from->id))
			{
				botDatabase.SetWarns(replyToMessage->from->id, groupSettings.id, previousQuantityWarn);

				return { "user could not be banned", "the user could not be banned" };
			}

			return { "user banned", "the user is banned" };
		}
		else if (newQuantityWarns >= groupSettings.numWarnToMute && newQuantityWarns < groupSettings.numWarnToBan && groupSettings.numWarnToMute > 0)
		{
			if (newQuantityWarns > previousQuantityWarn || (newQuantityWarns < previousQuantityWarn && memberStatus == "restricted"))
			{
				TgBot::ChatPermissions::Ptr permissions{ new TgBot::ChatPermissions };

				permissions->canSendMessages = false;
				permissions->canSendOtherMessages = false;
				permissions->canSendAudios = false;
				permissions->canSendDocuments = false;
				permissions->canSendPhotos = false;
				permissions->canSendPolls = false;
				permissions->canSendVideoNotes = false;
				permissions->canSendVideos = false;
				permissions->canSendVoiceNotes = false;
				permissions->canAddWebPagePreviews = false;

				auto untilTimePoint = std::chrono::system_clock::now() + std::chrono::hours(Fibonacci(newQuantityWarns - groupSettings.numWarnToMute) * 24);

				time_t untilTimestamp = std::chrono::system_clock::to_time_t(untilTimePoint);

				if (!bot.getApi().restrictChatMember(replyToMessage->chat->id, replyToMessage->from->id, permissions, untilTimestamp))
				{
					botDatabase.SetWarns(replyToMessage->from->id, groupSettings.id, previousQuantityWarn);

					return { "user could not be muted", "the user could not be muted" };
				}

				return { "user muted", "the user is muted" };
			}
			else if (newQuantityWarns < previousQuantityWarn)
			{
				if (memberStatus != "kicked") return { std::to_string(replyToMessage->from->id) + " (" + replyToMessage->from->username + ')' + " now has " + std::to_string(newQuantityWarns) + " warn",
					std::to_string(replyToMessage->from->id) + " (" + replyToMessage->from->username + ')' + " now has " + std::to_string(newQuantityWarns) + " warn" };

				if (!bot.getApi().unbanChatMember(replyToMessage->chat->id, replyToMessage->from->id, true))
				{
					botDatabase.SetWarns(replyToMessage->from->id, groupSettings.id, previousQuantityWarn);

					return { "user could not be unbanned", "the user could not be unbanned" };
				}

				return { "user unbanned", "the user is unbanned" };
			}
			else
				return { gmb::msg::unknownBehavior, gmb::msg::unknownBehavior };
		}
		else if (newQuantityWarns < groupSettings.numWarnToMute)
		{
			if (memberStatus == "restricted")
			{
				TgBot::ChatPermissions::Ptr permissions{ new TgBot::ChatPermissions };

				permissions->canSendMessages = true;
				permissions->canSendOtherMessages = true;
				permissions->canSendAudios = true;
				permissions->canSendDocuments = true;
				permissions->canSendPhotos = true;
				permissions->canSendPolls = true;
				permissions->canSendVideoNotes = true;
				permissions->canSendVideos = true;
				permissions->canSendVoiceNotes = true;
				permissions->canAddWebPagePreviews = true;

				if (!bot.getApi().restrictChatMember(replyToMessage->chat->id, replyToMessage->from->id, permissions))
				{
					botDatabase.SetWarns(replyToMessage->from->id, groupSettings.id, previousQuantityWarn);

					return { "user could not be unmuted", "the user could not be unmuted" };
				}

				return { "user unmuted", "the user is unmuted" };
			}
			else if (memberStatus == "kicked")
			{
				if (!bot.getApi().unbanChatMember(replyToMessage->chat->id, replyToMessage->from->id, true))
				{
					botDatabase.SetWarns(replyToMessage->from->id, groupSettings.id, previousQuantityWarn);

					return { "user could not be unbanned", "the user could not be unbanned" };
				}

				return { "user unbanned", "the user is unbanned" };
			}
			else
				return { std::to_string(replyToMessage->from->id) + " (" + replyToMessage->from->username + ')' + " now has " + std::to_string(newQuantityWarns) + " warn",
					std::to_string(replyToMessage->from->id) + " (" + replyToMessage->from->username + ')' + " now has " + std::to_string(newQuantityWarns) + " warn" };
		}
		else
			return { gmb::msg::unknownBehavior, gmb::msg::unknownBehavior };
	}

	logging::OnEventResult BotController::OnViewWarn(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { gmb::msg::log::privateChat, "" };

		if (!botDatabase.IsBotActive(message->chat->id)) return { gmb::msg::log::botIsNotActive,  gmb::msg::chat::botIsNotActive };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		const TgBot::Message::Ptr replyToMessage = message->replyToMessage;

		if (!replyToMessage) return { gmb::msg::log::notReplyToMessage,  gmb::msg::chat::notReplyToMessage };
		
		std::string textMessage{};

		textMessage += replyToMessage->from->firstName;
		textMessage += " has ";
		textMessage += std::to_string(botDatabase.GetWarns(replyToMessage->from->id, message->chat->id));
		textMessage += " warnings";

		return { "viewed warns " + replyToMessage->from->username + '(' + std::to_string(replyToMessage->from->id) + ')', "", textMessage};
	}

	logging::OnEventResult BotController::OnMyChatMember(TgBot::ChatMemberUpdated::Ptr update)
	{
		const bool isContains = botDatabase.GetGroups().contains(update->chat->id);
		const std::string status = update->newChatMember->status;

		if (!isContains && (status == "member" || status == "administrator"))
		{
			botDatabase.AddGroup(BotDatabase::Group{
			update->chat->id,
			update->chat->title,
			RandomNumberGenerator(32),
			update->chat->type,
			status == "administrator",
			false },
			BotDatabase::GroupSettings{
			update->chat->id,
			3,
			5 }
			);

			return { "bot added to group", "the bot has been added to the group"};
		}
		else if (isContains && (status == "member" || status == "administrator"))
		{
			const BotDatabase::Group& group = botDatabase.GetGroups().at(update->chat->id);
			const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(group.id);

			BotDatabase::Group updateGroup = group;
			updateGroup.isBotAdmin = status == "administrator";

			botDatabase.UpdateGroup(updateGroup, groupSettings);

			return { "bot became an " + status, "the bot became an " + status};
		}
		else if (isContains && (status == "left" || status == "kicked"))
		{
			botDatabase.DeleteGroup(update->chat->id);

			return { "bot left group", "the bot left the group" };
		}

		return { gmb::msg::unknownBehavior, gmb::msg::unknownBehavior };
	}

	int64_t BotController::Fibonacci(const size_t numberOfNumber) const
	{
		int64_t num = 1, previousNum = 1;

		for (size_t i = 1; i < numberOfNumber; ++i)
		{
			const int64_t temp = previousNum;

			previousNum = num;

			num += temp;
		}

		return num;
	}

	std::string BotController::RandomNumberGenerator(const size_t length)
	{
		std::string number;

		for (size_t a = 0; a < length; ++a)
		{
			number += static_cast<char>(uniform_dist(dre));
		}

		return number;
	}
}