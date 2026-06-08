#include "BotController.h"

#include <algorithm> 
#include <cctype>    
#include <chrono> 
#include <thread>

#ifdef _WIN32
#include <corecrt.h>
#endif

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

		AddBotCommand(gmb::consts::command::start, &BotController::OnStart);

		AddBotCommand(gmb::consts::command::botActive, &BotController::OnBotActive);
		AddBotCommand(gmb::consts::command::botDeactive, &BotController::OnBotDeactive);

		AddBotCommand(gmb::consts::command::groups, &BotController::OnGroups);
		AddBotCommand(gmb::consts::command::setGroupUniqueTitle, &BotController::OnSetGroupUniqueTitle);

		AddBotCommand(gmb::consts::command::admins, &BotController::OnAdmins);
		AddBotCommand(gmb::consts::command::addAdmin, &BotController::OnAddAdmin);
		AddBotCommand(gmb::consts::command::removeAdmin, &BotController::OnRemoveAdmin);

		AddBotCommand(gmb::consts::command::setWarnMuteSettings, &BotController::OnSetWarnMuteSettings);
		AddBotCommand(gmb::consts::command::setWarnBanSettings, &BotController::OnSetWarnBanSettings);

		AddBotCommand(gmb::consts::command::addWarn, &BotController::OnSetWarn);
		AddBotCommand(gmb::consts::command::removeWarn, &BotController::OnSetWarn);
		AddBotCommand(gmb::consts::command::setWarn, &BotController::OnSetWarn);
		AddBotCommand(gmb::consts::command::viewWarn, &BotController::OnViewWarn);

		AddBotCommand(gmb::consts::command::disableBot, &BotController::OnDisableBot);

		bot.getEvents().onMyChatMember([this](TgBot::ChatMemberUpdated::Ptr update) { SafeExecute(logging::LogSource::Bot, logging::LogType::Event, logging::ContextLog::ToContextLog(update, gmb::consts::event::botChatMemberUpdated), [this, update]() { return OnMyChatMember(update); }); });

		bot.getApi().setMyCommands(uiCommands);
	}

	void BotController::Run(const bool enableProcessPendingUpdates)
	{
		if (enableProcessPendingUpdates) ProcessPendingUpdates();

		gmb::logging::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::Event, [this]() {
			std::string text = "bot: \"";
			text += bot.getApi().getMe()->username;
			text += "\" has been launched";
			return text;
			}());

		TgBot::TgLongPoll longPoll(bot);

		while (botWorking)
		{
			SafeExecute(logging::LogSource::Program, logging::LogType::Event, logging::ContextLog{}, [&]() -> logging::OnEventResult {
				while (botWorking) { longPoll.start(); }
				return { "", "" }; });

			if(botWorking) std::this_thread::sleep_for(std::chrono::seconds(5));
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

/start - Show available commands
/disable_bot - Turn off the bot completely

Group Settings
/groups - List all groups containing the bot
/set_group_unique_title - Change the uniqueTitle for a group

Managing Bot Administrators
/admins - List all bot administrators
/add_admin - Generate an AdminConfirmationCode.
/remove_admin - Remove an admin using their index number from /admins

Warning Settings
/set_warn_mute_settings - Set the number of warnings after which a group member will be muted. Default: 3. Mute duration (days) = Fibonacci(UserWarns?QuantityWarnToMute)
/set_warn_ban_settings - Set the number of warnings before banning a group member. Default: 5

In a group:

Bot Management
/bot_active - Activates the bot. The bot begins executing commands in the group
/bot_deactive - Deactivate the bot. The bot stops executing commands in the group

Managing Warnings
/add_warn - Add the specified number of warnings to a member. Default: 1
/remove_warn - Remove the specified number of warnings to a member. Default: 1
/set_warn - Set the specified number of warnings for a member
/view_warn - Check the current number of warnings for a member
)" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { gmb::msg::log::fromAdmin, R"(
I can help you maintain discipline in your Telegram groups. Documentation (https://github.com/H-D-OWL/GroupModerBot).

You can control me by sending the following commands:

In a private chat:

/start - Show available commands

Group Settings
/groups - List all groups containing the bot

Managing Bot Administrators
/admins - List all bot administrators

Warning Settings
/set_warn_mute_settings - Set the number of warnings after which a group member will be muted. Default: 3. Mute duration (days) = Fibonacci(UserWarns?QuantityWarnToMute)
/set_warn_ban_settings - Set the number of warnings before banning a group member. Default: 5

In a group:

Managing Warnings
/add_warn - Add the specified number of warnings to a member. Default: 1
/remove_warn - Remove the specified number of warnings to a member. Default: 1
/set_warn - Set the specified number of warnings for a member
/view_warn - Check the current number of warnings for a member
)" };
		}
		else if (botDatabase.GetNumberAdmins() == 0)
		{
			return { gmb::msg::log::fromPossibleOwner, R"(
I can help you maintain discipline in your Telegram groups. Documentation (https://github.com/H-D-OWL/GroupModerBot).

You can control me by sending the following commands:

In a private chat:

/start - Show available commands

Managing Bot Administrators
/add_admin -  Become the owner by entering the confirmation code
)" };
		}
		else
		{
			return { gmb::msg::log::fromGuest, R"(
I can help you maintain discipline in your Telegram groups. Documentation (https://github.com/H-D-OWL/GroupModerBot).

You can control me by sending the following commands:

In a private chat:

/start - Show available commands

Managing Bot Administrators
/add_admin - Become an admin by entering the confirmation code
)" };
		}
	}

	logging::OnEventResult BotController::OnBotActive(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { gmb::msg::log::privateChat, "" };

		if (botDatabase.IsBotActive(message->chat->id)) return { gmb::msg::log::botIsActive,  gmb::msg::chat::botIsActive };

		if (botDatabase.IsOwner(message->from->id))
		{
			const BotDatabase::Group* group = botDatabase.GetGroup(message->chat->id);

			if (group == nullptr ) return { gmb::msg::log::groupNotFoundInCache, gmb::msg::log::groupNotFoundInCache };

			const BotDatabase::GroupSettings* groupSettings = botDatabase.GetGroupSettings(message->chat->id);

			if (groupSettings == nullptr) return { gmb::msg::log::groupSettingsNotFoundInCache, gmb::msg::log::groupSettingsNotFoundInCache };

			BotDatabase::Group updateGroup = *group;
			updateGroup.isBotActive = true;

			botDatabase.UpdateGroup(updateGroup, *groupSettings);

			return { "bot has been activated", "the bot has been activated" };
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
			const BotDatabase::Group* group = botDatabase.GetGroup(message->chat->id);

			if (group == nullptr) return { gmb::msg::log::groupNotFoundInCache, gmb::msg::log::groupNotFoundInCache };

			const BotDatabase::GroupSettings* groupSettings = botDatabase.GetGroupSettings(message->chat->id);

			if (groupSettings == nullptr) return { gmb::msg::log::groupSettingsNotFoundInCache, gmb::msg::log::groupSettingsNotFoundInCache };

			BotDatabase::Group updateGroup = *group;
			updateGroup.isBotActive = false;

			botDatabase.UpdateGroup(updateGroup, *groupSettings);

			return { "bot has been deactivated", "the bot has been deactivated" };
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

		std::stringstream commandParameters(message->text.substr(gmb::consts::command::setGroupUniqueTitle.size() + 1));

		std::string oldUniqueTitle{}, newUniqueTitle{};

		if (!(commandParameters >> oldUniqueTitle >> newUniqueTitle)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (newUniqueTitle.length() > 32) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (!std::all_of(newUniqueTitle.begin(), newUniqueTitle.end(), [](unsigned char c) { return isalnum(c) || c == '_'; })) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle))) return { gmb::msg::GroupWithUniqueTitleNotFound(oldUniqueTitle), gmb::msg::GroupWithUniqueTitleNotFound(oldUniqueTitle) };

		const BotDatabase::Group& group = botDatabase.GetGroups().at(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle));
		const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(group.id);

		BotDatabase::Group updateGroup{ group };
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
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (botDatabase.IsOwner(message->from->id))
		{
			confirmationCode = RandomNumberGenerator(32);

			return { "confirmation code generated", "confirmation code = " + confirmationCode };
		}
		
		if (botDatabase.IsAdmin(message->from->id))
		{
			return { gmb::msg::log::fromAdmin, gmb::msg::chat::cannotUseCommand };
		}

		std::stringstream commandParameters(message->text.substr(gmb::consts::command::addAdmin.size() + 1));

		std::string adminConfirmationCode{};

		if (!(commandParameters >> adminConfirmationCode)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		if (confirmationCode == gmb::consts::invalidTextData)
		{
			return { "confirmation code not generated", "confirmation code not generated" };
		}

		if (adminConfirmationCode != confirmationCode)
		{
			return { "confirmation code is incorrect", "the confirmation code is incorrect" };
		}

		const bool isOwner{ botDatabase.GetNumberAdmins() == 0 };

		botDatabase.AddAdmin(BotDatabase::Admin{
		message->from->id,
		message->from->firstName,
		message->from->lastName,
		message->from->username,
		message->from->isBot,
		message->from->isPremium,
		isOwner
			});

		confirmationCode = gmb::consts::invalidTextData;

		if(isOwner)
			return { "user became bot owner", "you have become a bot owner" };
		else
			return { "user became bot admin", "you have become a bot admin" };
	}

	logging::OnEventResult BotController::OnRemoveAdmin(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsOwner(message->from->id)) return { gmb::msg::log::notFromOwner, gmb::msg::chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr(gmb::consts::command::removeAdmin.size() + 1));

		size_t adminNumber{};

		if (!(commandParameters >> adminNumber)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		--adminNumber;

		if (!(adminNumber < botDatabase.GetNumberAdmins())) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

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
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr(gmb::consts::command::setWarnMuteSettings.size() + 1));

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

		return { "warn mute settings changed: " + std::to_string(updategroupSettings.numWarnToMute), "warn mute settings changed: " + std::to_string(updategroupSettings.numWarnToMute) };
	}

	logging::OnEventResult BotController::OnSetWarnBanSettings(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr(gmb::consts::command::setWarnBanSettings.size() + 1));

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

		return { "warn ban settings changed: " + std::to_string(updategroupSettings.numWarnToBan), "warn ban settings changed: " + std::to_string(updategroupSettings.numWarnToBan) };
	}

	logging::OnEventResult BotController::OnSetWarn(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { gmb::msg::log::privateChat, "" };

		if (!botDatabase.IsBotActive(message->chat->id)) return { gmb::msg::log::botIsNotActive,  gmb::msg::chat::botIsNotActive };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		const TgBot::Message::Ptr replyToMessage = message->replyToMessage;

		if (!replyToMessage) return { gmb::msg::log::notReplyToMessage,  gmb::msg::chat::notReplyToMessage };

		if (botDatabase.IsAdmin(replyToMessage->from->id)) return { "admins cannot influence each other", "admins cannot influence each other" };

#ifndef NDEBUG
#else
		if (replyToMessage->from->isBot) return { "bot cannot be given warnings", "the bot cannot be given warnings" };
#endif

		std::stringstream commandParameters(message->text);

		std::string comand{};

		if (!(commandParameters >> comand)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		const BotDatabase::GroupSettings& groupSettings = botDatabase.GetGroupsSettings().at(message->chat->id);

		const std::string memberStatus = bot.getApi().getChatMember(groupSettings.id, replyToMessage->from->id)->status;

		const int64_t previousQuantityWarn = botDatabase.GetWarns(replyToMessage->from->id, groupSettings.id);

		int64_t newQuantityWarns{}, arg{};

		if (const size_t pos = comand.find('@'); pos != comand.npos) { comand = comand.substr(0, pos); }

		if (!comand.empty() && comand[0] == '/') { comand = comand.substr(1); }

		if (comand == gmb::consts::command::addWarn)
		{
			if (!(commandParameters >> arg)) arg = 1;

			newQuantityWarns = previousQuantityWarn + arg;
		}
		else if (comand == gmb::consts::command::removeWarn)
		{
			if (!(commandParameters >> arg)) arg = 1;

			newQuantityWarns = previousQuantityWarn - arg;
		}
		else if (comand == gmb::consts::command::setWarn)
		{
			if (!(commandParameters >> newQuantityWarns)) return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };
		}
		else
			return { gmb::msg::log::invalidCommandParameters, gmb::msg::chat::invalidCommandParameters };

		newQuantityWarns = std::max(static_cast<int64_t>(0), newQuantityWarns);

		const bool banEnabled = groupSettings.numWarnToBan > 0;
		const bool muteEnabled = groupSettings.numWarnToMute > 0;

		const bool shouldBeBanned = banEnabled && (newQuantityWarns >= groupSettings.numWarnToBan);
		const bool shouldBeMuted = muteEnabled && (newQuantityWarns >= groupSettings.numWarnToMute) && !shouldBeBanned;
		const bool isSafe = !shouldBeBanned && !shouldBeMuted;

		std::string resultLogMsg{};
		std::string resultChatMsg{};

		if (shouldBeBanned) //ban
		{
			if (memberStatus == "kicked") //user already banned
			{
				resultLogMsg = "user already banned. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "the user is already banned. Warnings: " + std::to_string(newQuantityWarns);
			}
			else
			{
				if (!bot.getApi().banChatMember(replyToMessage->chat->id, replyToMessage->from->id))
					return { "user could not be banned", "the user could not be banned" }; //ban failed

				resultLogMsg = "user banned. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "the user is banned. Warnings: " + std::to_string(newQuantityWarns);
			}
		}
		else if (memberStatus == "left") 
		{
			resultLogMsg = gmb::msg::log::userNotInGroup + ". Warnings: " + std::to_string(newQuantityWarns);
			resultChatMsg = gmb::msg::chat::userNotInGroup + ". Warnings: " + std::to_string(newQuantityWarns);
		}
		else if(shouldBeMuted) //mute
		{
			if (newQuantityWarns == previousQuantityWarn && memberStatus == "restricted") //number warnings not changed
				return { "number warnings not changed: " + std::to_string(previousQuantityWarn), "the number of warnings has not changed: " + std::to_string(previousQuantityWarn) };

			if (memberStatus == "kicked" && !bot.getApi().unbanChatMember(replyToMessage->chat->id, replyToMessage->from->id, true)) 
				return { "user could not be unbanned to mute", "the user could not be unbanned to mute" }; //unban failed
			
			const auto untilTimePoint = std::chrono::system_clock::now() + std::chrono::hours(Fibonacci(newQuantityWarns - groupSettings.numWarnToMute) * 24);

			const time_t untilTimestamp = std::chrono::system_clock::to_time_t(untilTimePoint);

			if (!bot.getApi().restrictChatMember(replyToMessage->chat->id, replyToMessage->from->id, std::make_shared<TgBot::ChatPermissions>(false), static_cast<uint32_t>(untilTimestamp))) 
				return { "user could not be muted", "the user could not be muted" }; //mute failed
			
			resultLogMsg = "user muted / mute updated. Warnings: " + std::to_string(newQuantityWarns);
			resultChatMsg = "the user is muted. Warnings: " + std::to_string(newQuantityWarns);
		}
		else if(isSafe)
		{
			if (newQuantityWarns == previousQuantityWarn) 
				return { "number warnings not changed: " + std::to_string(previousQuantityWarn), "the number of warnings has not changed: " + std::to_string(previousQuantityWarn) }; //number warnings not changed

			if (memberStatus == "kicked")
			{
				if (!bot.getApi().unbanChatMember(replyToMessage->chat->id, replyToMessage->from->id, true)) 
					return { "user could not be unbanned", "the user could not be unbanned" }; //unban failed
				
				resultLogMsg = "user unbanned. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "the user is unbanned. Warnings: " + std::to_string(newQuantityWarns);
			}
			else if (memberStatus == "restricted")
			{
				if (!bot.getApi().restrictChatMember(replyToMessage->chat->id, replyToMessage->from->id, std::make_shared<TgBot::ChatPermissions>(true, true, true, true, true, true, true, true, true, true, true, true, true, true)))
					return { "user could not be unmuted", "the user could not be unmuted" }; //unmute failed
				
				resultLogMsg = "user unmuted. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "the user is unmuted. Warnings: " + std::to_string(newQuantityWarns);
			}
			else
			{
				resultLogMsg = "number warnings changed: " + std::to_string(newQuantityWarns);
				resultChatMsg = "the number of warnings has changed: " + std::to_string(newQuantityWarns);
			}
		}

		if (newQuantityWarns == 0)
			botDatabase.DeleteWarns(replyToMessage->from->id, groupSettings.id);
		else
			botDatabase.SetWarns(replyToMessage->from->id, groupSettings.id, newQuantityWarns);

		return { resultLogMsg, resultChatMsg };
	}

	logging::OnEventResult BotController::OnViewWarn(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { gmb::msg::log::privateChat, "" };

		if (!botDatabase.IsBotActive(message->chat->id)) return { gmb::msg::log::botIsNotActive,  gmb::msg::chat::botIsNotActive };

		if (!botDatabase.IsAdmin(message->from->id)) return { gmb::msg::log::fromGuest, gmb::msg::chat::cannotUseCommand };

		const TgBot::Message::Ptr replyToMessage = message->replyToMessage;

		if (!replyToMessage) return { gmb::msg::log::notReplyToMessage,  gmb::msg::chat::notReplyToMessage };
		
		const std::string quantityWarns{ std::to_string(botDatabase.GetWarns(replyToMessage->from->id, message->chat->id)) };

		std::string textMessage{};

		textMessage += replyToMessage->from->firstName;
		textMessage += " has ";
		textMessage += quantityWarns;
		textMessage += " warnings";

		return { "viewed warns " + std::to_string(replyToMessage->from->id) + " (" + replyToMessage->from->firstName + "). Warnings: " + quantityWarns, "", textMessage};
	}

	logging::OnEventResult BotController::OnDisableBot(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { gmb::msg::log::nonPrivateChat, "" };

		if (!botDatabase.IsOwner(message->from->id)) return { gmb::msg::log::notFromOwner, gmb::msg::chat::cannotUseCommand };

		botWorking = false;

		return { "", "the bot has stopped working" };
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
		else if (status == "left" || status == "kicked")
		{
			if(isContains)
				botDatabase.DeleteGroup(update->chat->id);

			return { "bot left group", "the bot left the group" };
		}

		assert(false && gmb::msg::unknownBehavior.c_str());

		return { gmb::msg::unknownBehavior + ": status: " + status + ", isContains: " + (isContains ? "true" : "false"), gmb::msg::unknownBehavior};
	}
	
	void BotController::ProcessPendingUpdates()
	{
		gmb::logging::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::Event, "Start ProcessPendingUpdates");

		int32_t offset{ 0 };
		bool processingCompleted{ false };

		int32_t numberMissedUpdates{ 0 };

		for (size_t i = 0; i < gmb::consts::numberAttemptsExecuteProcessPendingUpdates && !processingCompleted; ++i)
		{
			SafeExecute(logging::LogSource::Bot, logging::LogType::Event, {}, [this, &processingCompleted, &offset, &numberMissedUpdates]() -> logging::OnEventResult
				{
					bool stop{ false };

					while (!stop)
					{
						auto updates{ bot.getApi().getUpdates(offset) };

						if (updates.empty())
						{
							stop = true;
							break;
						}

						for (const auto& update : updates)
						{
							offset = update->updateId + 1;
							
							if (update->myChatMember)
								SafeExecute(logging::LogSource::Bot, logging::LogType::Event, logging::ContextLog::ToContextLog(update->myChatMember, gmb::consts::event::botChatMemberUpdated), [this, &processingCompleted, update]()->logging::OnEventResult
									{
										return OnMyChatMember(update->myChatMember);
									}, true);
							++numberMissedUpdates;
						}
					}

					if (offset > 0) bot.getApi().getUpdates(offset);

					processingCompleted = true;

					return {};
				}, true);

			if(!processingCompleted) std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		gmb::logging::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::Event, processingCompleted ? "Finished ProcessPendingUpdates. Missed \"Updates\": " + std::to_string(numberMissedUpdates) : "ProcessPendingUpdates failed");
	}

	int64_t BotController::Fibonacci(size_t numberOfNumber) const
	{
		if (numberOfNumber > 90) numberOfNumber = 90;

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
		thread_local std::random_device rd;
		thread_local std::default_random_engine dre{ rd() };
		std::uniform_int_distribution<int> uniform_dist{ '0', '9' };

		std::string number;

		for (size_t a = 0; a < length; ++a)
		{
			number += static_cast<char>(uniform_dist(dre));
		}

		return number;
	}
}