#include "BotController.h"

#include <algorithm> 
#include <atomic>
#include <cassert>
#include <cctype>    
#include <chrono> 
#include <csignal>
#include <cstdint> 
#include <cstdlib>
#include <memory>
#include <random>
#include <shared_mutex>  
#include <sstream> 
#include <string> 
#include <string_view> 
#include <thread>
#include <unordered_map>
#include <format>

#ifdef _WIN32
#include <Windows.h>
#include <consoleapi.h>
#include <corecrt.h>
#endif

#include <tgbot/Bot.h>
#include <tgbot/net/TgLongPoll.h> 
#include <tgbot/types/Chat.h> 
#include <tgbot/types/ChatMemberUpdated.h>
#include <tgbot/types/ChatPermissions.h> 
#include <tgbot/types/Message.h>

#include <taskflow/taskflow.hpp>

#include "BotDatabase.h"
#include "Constants.h"
#include "Logging.h"

namespace gmb
{
	namespace
	{
		namespace command
		{
			constexpr std::string_view start{ "start" };
			constexpr std::string_view botActive{ "bot_active" };
			constexpr std::string_view botDeactive{ "bot_deactive" };
			constexpr std::string_view groups{ "groups" };
			constexpr std::string_view setGroupUniqueTitle{ "set_group_unique_title" };
			constexpr std::string_view admins{ "admins" };
			constexpr std::string_view addAdmin{ "add_admin" };
			constexpr std::string_view removeAdmin{ "remove_admin" };
			constexpr std::string_view setWarnMuteSettings{ "set_warn_mute_settings" };
			constexpr std::string_view setWarnBanSettings{ "set_warn_ban_settings" };
			constexpr std::string_view addWarn{ "add_warn" };
			constexpr std::string_view removeWarn{ "remove_warn" };
			constexpr std::string_view setWarn{ "set_warn" };
			constexpr std::string_view viewWarn{ "view_warn" };
			constexpr std::string_view disableBot{ "disable_bot" };
		}

		namespace event
		{
			constexpr std::string_view botChatMemberUpdated = "bot_chat_member_updated";
		}

		namespace log
		{
			constexpr const char* privateChat{ "call in private chat" };
			constexpr const char* nonPrivateChat{ "call in non-private chat" };
			constexpr const char* fromOwner{ "call from owner" };
			constexpr const char* fromAdmin{ "call from admin" };
			constexpr const char* fromPossibleOwner{ "call from possible owner" };
			constexpr const char* fromGuest{ "call from guest" };
			constexpr const char* notFromOwner{ "call not from owner" };
			constexpr const char* invalidCommandParameters{ "invalid command parameters" };
			constexpr const char* notReplyToMessage{ "message is not reply to message" };
			constexpr const char* userNotInGroup{ "user addressed by command is not member of group" };
			constexpr const char* groupNotFoundInCache{ "group not found in cache" };
			constexpr const char* groupSettingsNotFoundInCache{ "group settings not found in cache" };
			constexpr const char* botIsActive{ "bot is active" };
			constexpr const char* botIsNotActive{ "bot is inactive" };
		}

		namespace chat
		{
			constexpr const char* cannotUseCommand{ "you cannot use this command" };
			constexpr const char* invalidCommandParameters{ "you entered the command parameters incorrectly" };
			constexpr const char* notReplyToMessage{ "message is not a reply to the message" };
			constexpr const char* userNotInGroup{ "the command is directed to is not in the group" };
			constexpr const char* botIsActive{ "the bot is active" };
			constexpr const char* botIsNotActive{ "the bot is inactive" };
		}

		inline std::string GroupWithUniqueTitleNotFound(const std::string_view uniqueTitle)
		{
			std::string text{ "group with uniqueTitle = \"" };
			text += uniqueTitle;
			text += "\" not found";

			return text;
		}
	}

	BotController* botController = nullptr;

	BotController::BotController(TgBot::Bot& bot, BotDatabase& botDatabase) : bot(bot), botDatabase(botDatabase)
	{
		if (botController) return;

		botController = this;

#ifdef _WIN32
		SetConsoleCtrlHandler(AppCloseHandler, TRUE);
#else
		std::signal(SIGINT, AppCloseHandler);
		std::signal(SIGTERM, AppCloseHandler);
		std::signal(SIGQUIT, AppCloseHandler);
#endif

		if (!botDatabase.GetNumberAdmins())
		{
			confirmationCode.clear();

			static constexpr size_t confirmationCodeLength{ 64 };

			confirmationCode = RandomNumberGenerator(confirmationCodeLength);

			logging::Logger::Log(logging::LogSource::Program, logging::LogType::Event, "confirmation code: " + confirmationCode);
		}

		AddBotCommand(std::string(command::start), &BotController::OnStart);

		AddBotCommand(std::string(command::botActive), &BotController::OnBotActive);
		AddBotCommand(std::string(command::botDeactive), &BotController::OnBotDeactive);

		AddBotCommand(std::string(command::groups), &BotController::OnGroups);
		AddBotCommand(std::string(command::setGroupUniqueTitle), &BotController::OnSetGroupUniqueTitle);

		AddBotCommand(std::string(command::admins), &BotController::OnAdmins);
		AddBotCommand(std::string(command::addAdmin), &BotController::OnAddAdmin);
		AddBotCommand(std::string(command::removeAdmin), &BotController::OnRemoveAdmin);

		AddBotCommand(std::string(command::setWarnMuteSettings), &BotController::OnSetWarnMuteSettings);
		AddBotCommand(std::string(command::setWarnBanSettings), &BotController::OnSetWarnBanSettings);

		AddBotCommand(std::string(command::addWarn), &BotController::OnSetWarn);
		AddBotCommand(std::string(command::removeWarn), &BotController::OnSetWarn);
		AddBotCommand(std::string(command::setWarn), &BotController::OnSetWarn);
		AddBotCommand(std::string(command::viewWarn), &BotController::OnViewWarn);

		AddBotCommand(std::string(command::disableBot), &BotController::OnDisableBot);

		bot.getEvents().onMyChatMember([this](TgBot::ChatMemberUpdated::Ptr update) { SafeExecute(logging::LogSource::Bot, logging::LogType::Event, logging::ContextLog::ToContextLog(update, std::string(event::botChatMemberUpdated)), [this, update]() { return OnMyChatMember(update); }); });

		bot.getApi().setMyCommands(uiCommands);
	}

	BotController::~BotController()
	{
		botController = nullptr;
	}

	void BotController::Run(const bool enableProcessPendingUpdates)
	{
		if (enableProcessPendingUpdates) ProcessPendingUpdates();

		gmb::logging::Logger::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::Event, std::format("bot: \"{}\" has been launched", bot.getApi().getMe()->username));

		static constexpr int32_t limitUpdatesAtTime{ 100 }, timeoutBetweenRequests{ 3 };

		TgBot::TgLongPoll longPoll(bot, limitUpdatesAtTime, timeoutBetweenRequests);

		static constexpr int64_t additionalDelayDurationOnError{ 5 };

		while (botWorking)
		{
			SafeExecute(logging::LogSource::Program, logging::LogType::Event, logging::ContextLog{}, [&]() -> logging::OnEventResult {
				while (botWorking) { longPoll.start(); }
				return { "" }; });

			if (botWorking) std::this_thread::sleep_for(std::chrono::seconds(additionalDelayDurationOnError));
		}

		executor.wait_for_all();
	}

	logging::OnEventResult BotController::OnStart(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (botDatabase.IsOwner(message->from->id))
		{
			return { log::fromOwner, R"(
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
/remove_admin - Remove an admin using their ID

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
			return { log::fromAdmin, R"(
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
			return { log::fromPossibleOwner, R"(
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
			return { log::fromGuest, R"(
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
		if (message->chat->type == TgBot::Chat::Type::Private) return { log::privateChat };

		if (botDatabase.IsBotActive(message->chat->id)) return { log::botIsActive, chat::botIsActive };

		if (botDatabase.IsOwner(message->from->id))
		{
			const std::shared_ptr<const BotDatabase::Group> group{ botDatabase.GetGroup(message->chat->id) };

			if (group == nullptr) return { log::groupNotFoundInCache,log::groupNotFoundInCache };

			const std::shared_ptr<const BotDatabase::GroupSettings> groupSettings{ botDatabase.GetGroupSettings(message->chat->id) };

			if (groupSettings == nullptr) return { log::groupSettingsNotFoundInCache,log::groupSettingsNotFoundInCache };

			BotDatabase::Group updateGroup{ *group };
			updateGroup.isBotActive = true;

			botDatabase.UpdateGroup(updateGroup, *groupSettings);

			return { "bot has been activated", "the bot has been activated" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { log::fromAdmin,chat::cannotUseCommand };
		}
		else
		{
			return { log::fromGuest,chat::cannotUseCommand };
		}
	}

	logging::OnEventResult BotController::OnBotDeactive(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { log::privateChat };

		if (!botDatabase.IsBotActive(message->chat->id)) return { log::botIsNotActive, chat::botIsNotActive };

		if (botDatabase.IsOwner(message->from->id))
		{
			const std::shared_ptr<const BotDatabase::Group> group{ botDatabase.GetGroup(message->chat->id) };

			if (group == nullptr) return { log::groupNotFoundInCache,log::groupNotFoundInCache };

			const std::shared_ptr<const BotDatabase::GroupSettings> groupSettings{ botDatabase.GetGroupSettings(message->chat->id) };

			if (groupSettings == nullptr) return { log::groupSettingsNotFoundInCache,log::groupSettingsNotFoundInCache };

			BotDatabase::Group updateGroup{ *group };
			updateGroup.isBotActive = false;

			botDatabase.UpdateGroup(updateGroup, *groupSettings);

			return { "bot has been deactivated", "the bot has been deactivated" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { log::fromAdmin,chat::cannotUseCommand };
		}
		else
		{
			return { log::fromGuest,chat::cannotUseCommand };
		}
	}

	logging::OnEventResult BotController::OnGroups(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (!botDatabase.IsAdmin(message->from->id)) return { log::fromGuest,chat::cannotUseCommand };

		std::string sendMessageText{};

		size_t groupNumber{ 1 };

		for (const auto& [id, groupFromCache] : botDatabase.GetGroups())
		{
			sendMessageText += std::format("{}. {} ({}):\n    IsBotAdmin: {}\n    IsBotActive: {}\n    ", groupNumber, groupFromCache->title, groupFromCache->uniqueTitle,
				groupFromCache->isBotAdmin ? "Yes" : "No",
				groupFromCache->isBotActive ? "Yes" : "No");

			const std::shared_ptr<const BotDatabase::GroupSettings> groupSettingsFromCache{ botDatabase.GetGroupSettings(id) };

			if (groupSettingsFromCache)
			{
				sendMessageText += std::format("NumWarnToMute: {}\n    NumWarnToBan: {}\n    ", groupSettingsFromCache->numWarnToMute, groupSettingsFromCache->numWarnToBan);
			}

			sendMessageText += '\n';

			++groupNumber;
		}

		if (groupNumber == 1)
		{
			sendMessageText = "There are no groups";
		}

		return { "list of groups has been viewed", sendMessageText };
	}

	logging::OnEventResult BotController::OnSetGroupUniqueTitle(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (!botDatabase.IsOwner(message->from->id)) return { log::notFromOwner,chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr(command::setGroupUniqueTitle.size() + 1));

		std::string oldUniqueTitle{}, newUniqueTitle{};

		if (!(commandParameters >> oldUniqueTitle >> newUniqueTitle)) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		static constexpr size_t maxLengthUniqueTitle{ 32 };

		if (newUniqueTitle.length() > maxLengthUniqueTitle) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		if (!std::all_of(newUniqueTitle.begin(), newUniqueTitle.end(), [](unsigned char c) { return isalnum(c) || c == '_'; })) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle))) return { GroupWithUniqueTitleNotFound(oldUniqueTitle), GroupWithUniqueTitleNotFound(oldUniqueTitle) };

		const std::shared_ptr<const BotDatabase::Group> group{ botDatabase.GetGroup(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle)) };

		if (group == nullptr) return { log::groupNotFoundInCache,log::groupNotFoundInCache };

		const std::shared_ptr<const BotDatabase::GroupSettings> groupSettings{ botDatabase.GetGroupSettings(group->id) };

		if (groupSettings == nullptr) return { log::groupSettingsNotFoundInCache,log::groupSettingsNotFoundInCache };

		BotDatabase::Group updateGroup{ *group };
		updateGroup.uniqueTitle = newUniqueTitle;

		botDatabase.UpdateGroup(updateGroup, *groupSettings);

		return { std::format("unique title changed from {} to {}", oldUniqueTitle, newUniqueTitle), std::format("unique title \"{}\" changed from {} to {}", updateGroup.title, oldUniqueTitle, newUniqueTitle) };
	}

	logging::OnEventResult BotController::OnAdmins(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (!botDatabase.IsAdmin(message->from->id)) return { log::fromGuest,chat::cannotUseCommand };

		std::string sendMessageText{};

		size_t adminsNumber{ 1 };

		for (const auto& [id, adminFromCache] : botDatabase.GetAdmins())
		{
			sendMessageText += std::format("{}. {} ({} {}):\n    ID: {}\n    IsBotOwner: {}\n    IsBot: {}\n    IsPremium: {}\n\n", adminsNumber, adminFromCache->username, adminFromCache->firstName, adminFromCache->lastName,
				adminFromCache->id,
				adminFromCache->isBotOwner ? "Yes" : "No",
				adminFromCache->isBot ? "Yes" : "No",
				adminFromCache->isPremium ? "Yes" : "No");

			++adminsNumber;
		}

		if (adminsNumber == 1)
		{
			sendMessageText = "there are no admins";
		}

		return { "list of admins has been viewed", sendMessageText };
	}

	logging::OnEventResult BotController::OnAddAdmin(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		std::scoped_lock lock(mutexBotController);

		if (botDatabase.IsOwner(message->from->id))
		{
			static constexpr size_t adminConfirmationCodeLength{ 32 };

			confirmationCode = RandomNumberGenerator(adminConfirmationCodeLength);

			return { "confirmation code generated", "confirmation code = " + confirmationCode };
		}

		if (botDatabase.IsAdmin(message->from->id))
		{
			return { log::fromAdmin,chat::cannotUseCommand };
		}

		std::stringstream commandParameters(message->text.substr(command::addAdmin.size() + 1));

		std::string adminConfirmationCode{};

		if (!(commandParameters >> adminConfirmationCode)) return { log::invalidCommandParameters,chat::invalidCommandParameters };

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

		if (isOwner)
			return { "user became bot owner", "you have become a bot owner" };
		else
			return { "user became bot admin", "you have become a bot admin" };
	}

	logging::OnEventResult BotController::OnRemoveAdmin(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (!botDatabase.IsOwner(message->from->id)) return { log::notFromOwner,chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr(command::removeAdmin.size() + 1));

		int64_t adminId{};

		if (!(commandParameters >> adminId)) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		if (botDatabase.IsOwner(adminId)) return { "attempt to remove bot owner", "the bot owner cannot be removed" };

		const std::shared_ptr<const BotDatabase::Admin> removedAdmin = botDatabase.GetAdmin(adminId);

		if (!removedAdmin) return { "attempt to remove non-existent admin", "an admin with this id was not found" };

		botDatabase.DeleteAdmin(adminId);

		return { "admin has been removed", std::format("admin {} has been removed", ChatMessage({"", "", "",  removedAdmin->username, removedAdmin->firstName, removedAdmin->lastName }, "")) };
	}

	logging::OnEventResult BotController::OnSetWarnMuteSettings(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (!botDatabase.IsAdmin(message->from->id)) return { log::fromGuest,chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr(command::setWarnMuteSettings.size() + 1));

		std::string uniqueTitle{};

		int64_t numWarnToMute{};

		if (!(commandParameters >> uniqueTitle >> numWarnToMute)) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		if (numWarnToMute < 0) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(uniqueTitle))) return { GroupWithUniqueTitleNotFound(uniqueTitle), GroupWithUniqueTitleNotFound(uniqueTitle) };

		const std::shared_ptr<const BotDatabase::Group> group{ botDatabase.GetGroup(botDatabase.GroupIdFromUniqueTitle(uniqueTitle)) };

		if (group == nullptr) return { log::groupNotFoundInCache,log::groupNotFoundInCache };

		const std::shared_ptr<const BotDatabase::GroupSettings> groupSettings{ botDatabase.GetGroupSettings(group->id) };

		if (groupSettings == nullptr) return { log::groupSettingsNotFoundInCache,log::groupSettingsNotFoundInCache };

		BotDatabase::GroupSettings updateGroupSettings{ *groupSettings };
		updateGroupSettings.numWarnToMute = numWarnToMute;

		botDatabase.UpdateGroup(*group, updateGroupSettings);

		return { std::format("warn mute settings changed: {}", updateGroupSettings.numWarnToMute), std::format("{}: warn mute settings changed: {}", group->title, updateGroupSettings.numWarnToMute) };
	}

	logging::OnEventResult BotController::OnSetWarnBanSettings(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (!botDatabase.IsAdmin(message->from->id)) return { log::fromGuest,chat::cannotUseCommand };

		std::stringstream commandParameters(message->text.substr(command::setWarnBanSettings.size() + 1));

		std::string uniqueTitle{};

		int64_t numWarnToBan{};

		if (!(commandParameters >> uniqueTitle >> numWarnToBan)) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		if (numWarnToBan < 0) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(uniqueTitle))) return { GroupWithUniqueTitleNotFound(uniqueTitle), GroupWithUniqueTitleNotFound(uniqueTitle) };

		const std::shared_ptr<const BotDatabase::Group> group{ botDatabase.GetGroup(botDatabase.GroupIdFromUniqueTitle(uniqueTitle)) };

		if (group == nullptr) return { log::groupNotFoundInCache,log::groupNotFoundInCache };

		const std::shared_ptr<const BotDatabase::GroupSettings> groupSettings{ botDatabase.GetGroupSettings(group->id) };

		if (groupSettings == nullptr) return { log::groupSettingsNotFoundInCache,log::groupSettingsNotFoundInCache };

		BotDatabase::GroupSettings updateGroupSettings{ *groupSettings };
		updateGroupSettings.numWarnToBan = numWarnToBan;

		botDatabase.UpdateGroup(*group, updateGroupSettings);

		return { std::format("warn ban settings changed: {}", updateGroupSettings.numWarnToBan), std::format("{}: warn ban settings changed: {}", group->title, updateGroupSettings.numWarnToBan) };
	}

	logging::OnEventResult BotController::OnSetWarn(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { log::privateChat };

		if (!botDatabase.IsBotActive(message->chat->id)) return { log::botIsNotActive, chat::botIsNotActive };

		if (!botDatabase.IsAdmin(message->from->id)) return { log::fromGuest,chat::cannotUseCommand };

		const TgBot::Message::Ptr replyToMessage{ message->replyToMessage };

		if (!replyToMessage) return { log::notReplyToMessage, chat::notReplyToMessage };

		if (botDatabase.IsAdmin(replyToMessage->from->id)) return { "admins cannot influence each other", "admins cannot influence each other" };

#ifndef NDEBUG
#else
		if (replyToMessage->from->isBot) return { "bot cannot be given warnings", "the bot cannot be given warnings" };
#endif

		std::stringstream commandParameters(message->text);

		std::string command{};

		if (!(commandParameters >> command)) return { log::invalidCommandParameters,chat::invalidCommandParameters };

		const std::shared_ptr<const BotDatabase::GroupSettings> groupSettings{ botDatabase.GetGroupSettings(message->chat->id) };

		if (groupSettings == nullptr) return { log::groupSettingsNotFoundInCache,log::groupSettingsNotFoundInCache };

		const std::string memberStatus{ bot.getApi().getChatMember(groupSettings->id, replyToMessage->from->id)->status };

		const int64_t previousQuantityWarn{ botDatabase.GetWarns(replyToMessage->from->id, groupSettings->id) };

		int64_t newQuantityWarns{}, quantityWarns{};

		if (const size_t pos{ command.find('@') }; pos != command.npos) { command = command.substr(0, pos); }

		if (command.starts_with('/')) { command = command.substr(1); }

		static constexpr int64_t defaultQuantityWarns{ 1 };

		if (command == command::addWarn)
		{
			if (!(commandParameters >> quantityWarns)) quantityWarns = defaultQuantityWarns;

			newQuantityWarns = previousQuantityWarn + quantityWarns;
		}
		else if (command == command::removeWarn)
		{
			if (!(commandParameters >> quantityWarns)) quantityWarns = defaultQuantityWarns;

			newQuantityWarns = previousQuantityWarn - quantityWarns;
		}
		else if (command == command::setWarn)
		{
			if (!(commandParameters >> newQuantityWarns)) return { log::invalidCommandParameters,chat::invalidCommandParameters };
		}
		else
			return { log::invalidCommandParameters,chat::invalidCommandParameters };

		newQuantityWarns = std::max(static_cast<int64_t>(0), newQuantityWarns);

		const bool banEnabled{ groupSettings->numWarnToBan > 0 };
		const bool muteEnabled{ groupSettings->numWarnToMute > 0 };

		const bool shouldBeBanned{ banEnabled && (newQuantityWarns >= groupSettings->numWarnToBan) };
		const bool shouldBeMuted{ muteEnabled && (newQuantityWarns >= groupSettings->numWarnToMute) && !shouldBeBanned };
		const bool isSafe{ !shouldBeBanned && !shouldBeMuted };

		std::string resultLogMsg{};
		std::string resultChatMsg{};

		if (shouldBeBanned) //ban
		{
			if (memberStatus == "kicked") //user already banned
			{
				resultLogMsg = "user already banned. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "is already banned. Warnings: " + std::to_string(newQuantityWarns);
			}
			else
			{
				if (!bot.getApi().banChatMember(replyToMessage->chat->id, replyToMessage->from->id))
					return { "user could not be banned", "could not be banned" }; //ban failed

				resultLogMsg = "user banned. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "has been banned. Warnings: " + std::to_string(newQuantityWarns);
			}
		}
		else if (memberStatus == "left")
		{
			resultLogMsg = std::format("{}. Warnings: {}", log::userNotInGroup, newQuantityWarns);
			resultChatMsg = std::format("{}. Warnings: {}", chat::userNotInGroup, newQuantityWarns);
		}
		else if (shouldBeMuted) //mute
		{
			if (newQuantityWarns == previousQuantityWarn && memberStatus == "restricted") //number warnings not changed
				return { "number warnings not changed: " + std::to_string(previousQuantityWarn), "the number of warnings has not changed: " + std::to_string(previousQuantityWarn) };

			if (memberStatus == "kicked" && !bot.getApi().unbanChatMember(replyToMessage->chat->id, replyToMessage->from->id, true))
				return { "user could not be unbanned to mute", "could not be unbanned to mute" }; //unban failed

			const auto untilTimePoint{ std::chrono::system_clock::now() + std::chrono::hours(Fibonacci(newQuantityWarns - groupSettings->numWarnToMute > 14 ? 14 : newQuantityWarns - groupSettings->numWarnToMute) * 24) };

			const time_t untilTimestamp{ std::chrono::system_clock::to_time_t(untilTimePoint) };

			if (!bot.getApi().restrictChatMember(replyToMessage->chat->id, replyToMessage->from->id, std::make_shared<TgBot::ChatPermissions>(false), static_cast<uint32_t>(untilTimestamp)))
				return { "user could not be muted", "could not be muted" }; //mute failed

			resultLogMsg = "user muted / mute updated. Warnings: " + std::to_string(newQuantityWarns);
			resultChatMsg = "has been muted. Warnings: " + std::to_string(newQuantityWarns);
		}
		else if (isSafe)
		{
			if (newQuantityWarns == previousQuantityWarn)
				return { "number warnings not changed: " + std::to_string(previousQuantityWarn), "the number of warnings has not changed: " + std::to_string(previousQuantityWarn) }; //number warnings not changed

			if (memberStatus == "kicked")
			{
				if (!bot.getApi().unbanChatMember(replyToMessage->chat->id, replyToMessage->from->id, true))
					return { "user could not be unbanned", "could not be unbanned" }; //unban failed

				resultLogMsg = "user unbanned. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "has been unbanned. Warnings: " + std::to_string(newQuantityWarns);
			}
			else if (memberStatus == "restricted")
			{
				if (!bot.getApi().restrictChatMember(replyToMessage->chat->id, replyToMessage->from->id, std::make_shared<TgBot::ChatPermissions>(true, true, true, true, true, true, true, true, true, true, true, true, true, true)))
					return { "user could not be unmuted", "the user could not be unmuted" }; //unmute failed

				resultLogMsg = "user unmuted. Warnings: " + std::to_string(newQuantityWarns);
				resultChatMsg = "has been unmuted. Warnings: " + std::to_string(newQuantityWarns);
			}
			else
			{
				resultLogMsg = "number warnings changed: " + std::to_string(newQuantityWarns);
				resultChatMsg = "the number of warnings has changed: " + std::to_string(newQuantityWarns);
			}
		}

		if (newQuantityWarns == 0)
			botDatabase.DeleteWarns(replyToMessage->from->id, groupSettings->id);
		else
			botDatabase.SetWarns(replyToMessage->from->id, groupSettings->id, newQuantityWarns);

		return { resultLogMsg, resultChatMsg };
	}

	logging::OnEventResult BotController::OnViewWarn(TgBot::Message::Ptr message)
	{
		if (message->chat->type == TgBot::Chat::Type::Private) return { log::privateChat };

		if (!botDatabase.IsBotActive(message->chat->id)) return { log::botIsNotActive, chat::botIsNotActive };

		if (!botDatabase.IsAdmin(message->from->id)) return { log::fromGuest,chat::cannotUseCommand };

		const TgBot::Message::Ptr replyToMessage{ message->replyToMessage };

		if (!replyToMessage) return { log::notReplyToMessage, chat::notReplyToMessage };

		const int64_t quantityWarns{ botDatabase.GetWarns(replyToMessage->from->id, message->chat->id) };

		return { std::format("Warnings: {}", quantityWarns),  std::format("has {} warnings", quantityWarns) };
	}

	logging::OnEventResult BotController::OnDisableBot(TgBot::Message::Ptr message)
	{
		if (message->chat->type != TgBot::Chat::Type::Private) return { log::nonPrivateChat };

		if (!botDatabase.IsOwner(message->from->id)) return { log::notFromOwner,chat::cannotUseCommand };

		botWorking.store(false);

		return { "", "the bot has stopped working" };
	}

	logging::OnEventResult BotController::OnMyChatMember(TgBot::ChatMemberUpdated::Ptr update)
	{
		const bool containsGroup{ botDatabase.GetGroups().contains(update->chat->id) };
		const std::string status{ update->newChatMember->status };

		if (!containsGroup && (status == "member" || status == "administrator"))
		{
			static constexpr size_t lengthGeneratedUniqueTitle{ 32 };
			static constexpr int64_t startingNumWarnToMute{ 3 }, startingNumWarnToBan{ 5 };

			botDatabase.AddGroup(BotDatabase::Group{
			update->chat->id,
			update->chat->title,
			RandomNumberGenerator(lengthGeneratedUniqueTitle),
			update->chat->type,
			status == "administrator",
			false },
			BotDatabase::GroupSettings{
			update->chat->id,
			startingNumWarnToMute,
			startingNumWarnToBan }
			);

			return { "bot added to group", "the bot has been added to the group" };
		}
		else if (containsGroup && (status == "member" || status == "administrator"))
		{
			const std::shared_ptr<const BotDatabase::Group> group{ botDatabase.GetGroup(update->chat->id) };

			if (group == nullptr) return { log::groupNotFoundInCache,log::groupNotFoundInCache };

			const std::shared_ptr<const BotDatabase::GroupSettings> groupSettings{ botDatabase.GetGroupSettings(group->id) };

			if (groupSettings == nullptr) return { log::groupSettingsNotFoundInCache,log::groupSettingsNotFoundInCache };

			BotDatabase::Group updateGroup{ *group };
			updateGroup.isBotAdmin = status == "administrator";

			botDatabase.UpdateGroup(updateGroup, *groupSettings);

			return { "bot became an " + status, "the bot became an " + status };
		}
		else if (status == "left" || status == "kicked")
		{
			if (containsGroup)
				botDatabase.DeleteGroup(update->chat->id);

			return { "bot left group", "the bot left the group" };
		}

		assert(false && gmb::consts::msg::unknownBehavior.c_str());

		return { std::format("{}: status: {}, containsGroup: {}", gmb::consts::msg::unknownBehavior, status, containsGroup), gmb::consts::msg::unknownBehavior };
	}

	inline std::string BotController::GetShortDescription(const std::string_view command)
	{
		static const std::unordered_map<std::string_view, std::string> shortDescription{
			{ command::start, "Show available commands" },
			{ command::botActive, "Activates the bot. The bot begins executing commands in the group" },
			{ command::botDeactive, "Deactivate the bot. The bot stops executing commands in the group" },
			{ command::groups, "List all groups containing the bot" },
			{ command::setGroupUniqueTitle, "Change the uniqueTitle for a group" },
			{ command::admins, "List all bot administrators" },
			{ command::addAdmin, "Owner: Generate an AdminConfirmationCode. Guest: Become the owner (if none exists) or an admin by entering the confirmation code" },
			{ command::removeAdmin, "Remove an admin using their ID" },
			{ command::setWarnMuteSettings, "Set the number of warnings after which a group member will be muted. Default: 3. Mute duration (days) = Fibonacci(UserWarns-QuantityWarnToMute)" },
			{ command::setWarnBanSettings, "Set the number of warnings before banning a group member. Default: 5" },
			{ command::addWarn, "Add the specified number of warnings to a member. Default: 1" },
			{ command::removeWarn, "Remove the specified number of warnings to a member. Default: 1" },
			{ command::setWarn, "Set the specified number of warnings for a member" },
			{ command::viewWarn, "Check the current number of warnings for a member" },
			{ command::disableBot, "Turn off the bot completely" }
		};

		if (const auto it = shortDescription.find(command); it != shortDescription.cend())
			return it->second;
		else
		{
			assert(false && "Unknown command");
			return {};
		}
	}

	std::string BotController::ChatMessage(const logging::ContextLog& contextLog, const std::string& chatMsg)
	{
		std::string message{};

		if (!contextLog.title.empty())
		{
			message += contextLog.title;
			message += ':';
		}

		std::string userTarget{};

		if (!contextLog.userTargetFirstName.empty())
		{
			userTarget += contextLog.userTargetFirstName;
		}

		if (!contextLog.userTargetLastName.empty())
		{
			if (!userTarget.empty()) userTarget += ' ';

			userTarget += contextLog.userTargetLastName;
		}

		if (!contextLog.userTargetName.empty())
		{
			if (!userTarget.empty()) userTarget += ' ';

			userTarget += '(';
			userTarget += contextLog.userTargetName;
			userTarget += ')';
		}

		if (!userTarget.empty())
		{
			if (!message.empty()) message += ' ';

			message += userTarget;
			message += " -";
		}

		if (!message.empty()) message += ' ';

		message += chatMsg;

		return message;
	}

	void BotController::ProcessPendingUpdates()
	{
		gmb::logging::Logger::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::Event, "Start ProcessPendingUpdates");

		int32_t offset{};
		bool processingCompleted{ false };

		int32_t numberMissedUpdates{};

		static constexpr size_t numberAttemptsExecuteProcessPendingUpdates{ 3 };
		static constexpr int64_t delayDurationOnError{ 1 };

		for (size_t i{}; i < numberAttemptsExecuteProcessPendingUpdates && !processingCompleted && botWorking; ++i)
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
							if (!botWorking)
							{
								stop = true;
								break;
							}

							offset = update->updateId + 1;

							if (update->myChatMember)
								SafeExecute(logging::LogSource::Bot, logging::LogType::Event, logging::ContextLog::ToContextLog(update->myChatMember, std::string(event::botChatMemberUpdated)), [this, update]()->logging::OnEventResult
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

			if (!processingCompleted) std::this_thread::sleep_for(std::chrono::seconds(delayDurationOnError));
		}

		gmb::logging::Logger::Log(gmb::logging::LogSource::Bot, gmb::logging::LogType::Event, processingCompleted ? "Finished ProcessPendingUpdates. Missed \"Updates\": " + std::to_string(numberMissedUpdates) : "ProcessPendingUpdates failed");
	}

#ifdef _WIN32
	BOOL __stdcall BotController::AppCloseHandler(DWORD ctrlType)
	{
		if (botController && (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT)) {

			botController->botWorking.store(false);

			static constexpr int64_t durationDelayBeforeAppClosure{ 5 };

			std::this_thread::sleep_for(std::chrono::seconds(durationDelayBeforeAppClosure));

			return TRUE;
		}

		return FALSE;
	}
#else
	void BotController::AppCloseHandler(int signum)
	{
		if (botController && (signum == SIGINT || signum == SIGTERM || signum == SIGQUIT))
		{
			botController->botWorking.store(false);
		}
	}
#endif

	int64_t BotController::Fibonacci(size_t sequenceNumber) const noexcept
	{
		static constexpr size_t maxSequenceNumber{ 90 };

		if (sequenceNumber > maxSequenceNumber) sequenceNumber = maxSequenceNumber;

		int64_t num{ 1 }, previousNum{ 1 };

		for (size_t i{ 1 }; i < sequenceNumber; ++i)
		{
			const int64_t temp{ previousNum };

			previousNum = num;

			num += temp;
		}

		return num;
	}

	std::string BotController::RandomNumberGenerator(const size_t length)
	{
		thread_local std::random_device rd;
		thread_local std::mt19937_64 dre{ rd() };
		std::uniform_int_distribution<int> uniform_dist{ '0', '9' };

		std::string number;
		number.reserve(length);

		for (size_t a{}; a < length; ++a)
		{
			number += static_cast<char>(uniform_dist(dre));
		}

		return number;
	}
}