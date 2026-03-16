#include "BotController.h"

BotController::BotController(Bot& bot, BotDatabase& botDatabase) : bot(bot), botDatabase(botDatabase)
{
	if(!botDatabase.GetNumberAdmins())
	{
		confirmationCode.clear();

		confirmationCode = RandomNumberGenerator(64);

		Log(LogSource::Program, LogType::Event, "confirmation code: " + confirmationCode);
	}

	bot.getEvents().onCommand("start",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "start"),				[&](){ return OnStart(message); }); });
	bot.getEvents().onCommand("botActive",				[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "botActive"),			[&](){ return OnBotActive(message); }); });
	bot.getEvents().onCommand("botDeactive",			[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "botDeactive"),			[&](){ return OnBotDeactive(message); }); });
	bot.getEvents().onCommand("groups",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "groups"),				[&](){ return OnGroups(message); }); });
	bot.getEvents().onCommand("setGroupUniqueTitle",	[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "setGroupUniqueTitle"),	[&](){ return OnSetGroupUniqueTitle(message); }); });
	
	bot.getEvents().onCommand("ban",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "ban"),					[&](){ return OnBan(message); }); });
	bot.getEvents().onCommand("unban",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "unban"),				[&](){ return OnUnban(message); }); });
	bot.getEvents().onCommand("mute",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "mute"),				[&](){ return OnMute(message); }); });
	bot.getEvents().onCommand("unmute",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "unmute"),				[&](){ return OnUnmute(message); }); });
	
	bot.getEvents().onMyChatMember	(					[this](ChatMemberUpdated::Ptr update)	{ SafeExecute(ContextLog::ToContextLog(update,	"changeMyChatMember"),	[&](){ return onMyChatMember(update); }); });

}

void BotController::Run()
{
	TgLongPoll longPoll(bot);

	while (true)
		SafeExecute(ContextLog{}, [&]() -> OnEventResult {
		while (true) { longPoll.start(); }
		return { "", "" }; });
}

OnEventResult BotController::OnStart(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		if (botDatabase.GetNumberAdmins() == 0)
			return { "call by possible owner", "Hello. I'm " + bot.getApi().getMe()->username + ". I don't have an owner yet. To become one, enter the /addBotAdmin command and the confirmation code from the console" };
		else if (const auto admin = botDatabase.GetAdmin(message->from->id); admin != nullptr)
		{
			if (admin->isBotOwner)
				return { "call from owner", "Template response for the owner (debug)" };
			else
				return { "сall from admin", "Template response for the admin (debug)" };
		}
		else
			return { "сall from not admin", "Hello. I'm " + bot.getApi().getMe()->username + ". To become a bot admin, type the command /addBotAdmin and the confirmation code that the bot owner will provide you" };
	}
	else
		return { "сall in non-private chat", "" };
}

/*OnEventResult BotController::OnStart(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

		if (confirmationCode != "ERROR" && code == confirmationCode)
		{
			botDatabase.AddAdmin(BotDatabase::Admin{
			.id			= message->from->id,
			.firstName	= message->from->firstName,
			.lastName	= message->from->lastName,
			.username	= message->from->username,
			.isBot		= message->from->isBot,
			.isPremium	= message->from->isPremium,
			.isBotOwner	= true
				});

			confirmationCode = "ERROR";

			return { "confirmation code is correct", "You have become a bot admin" };
		}
		else if (confirmationCode != "ERROR" && !code.empty())
			return { "confirmation code is incorrect", "The confirmation code is incorrect" };
		else
			return { "confirmation code not entered", (botDatabase.IsAdmin(message->from->id) ? "You are the bot admin" : "Enter confirmation code")};
	}
	else
		return {"сalling in a non-private chat", ""};
}*/

OnEventResult BotController::OnBotActive(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
		botDatabase.UpdateGroup(BotDatabase::Group{
		.id				= message->chat->id,
		.title			= message->chat->title,
		.uniqueTitle	= (*botDatabase.GetGroups().find(message->chat->id)).second.uniqueTitle,
		.type			= message->chat->type,
		.isBotAdmin		= (*botDatabase.GetGroups().find(message->chat->id)).second.isBotAdmin,
		.isBotActive	= true
			});

	return { "test", "test" };

}

OnEventResult BotController::OnBotDeactive(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
		botDatabase.UpdateGroup(BotDatabase::Group{
		.id				= message->chat->id,
		.title			= message->chat->title,
		.uniqueTitle = (*botDatabase.GetGroups().find(message->chat->id)).second.uniqueTitle,
		.type			= message->chat->type,
		.isBotAdmin		= (*botDatabase.GetGroups().find(message->chat->id)).second.isBotAdmin,
		.isBotActive	= false
			});
	return { "test", "test" };

}

OnEventResult BotController::OnGroups(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		string sendMessageText{};

		size_t number{ 1 };

		for (const auto& [id, group] : botDatabase.GetGroups())
		{
			sendMessageText += to_string(number);
			sendMessageText += ". ";
			sendMessageText += group.title + " (" + group.uniqueTitle + ")" + ":\n    IsBotAdmin: " + (group.isBotAdmin ? "Yes" : "No") + "\n    " + "IsBotActive: " + (group.isBotActive ? "Yes" : "No");
			sendMessageText += '\n';
			++number;
		}

		if (number == 1)
		{
			sendMessageText = "There are no groups";
		}

		Log(LogSource::Bot, LogType::Event, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates");

		bot.getApi().sendMessage(message->chat->id, sendMessageText);
	}
	return { "test", "test" };

}

OnEventResult BotController::OnSetGroupUniqueTitle(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		string commandParameters = message->text.substr("/setGroupUniqueTitle"sv.size());
		string oldUniqueTitle = commandParameters.substr(commandParameters.find('\n') + 1);
		oldUniqueTitle = oldUniqueTitle.substr(0, oldUniqueTitle.find('\n'));
		string newUniqueTitle = commandParameters.substr(commandParameters.rfind('\n') + 1);

		oldUniqueTitle = CleaningUpLateralSpaces(oldUniqueTitle);
		newUniqueTitle = CleaningUpLateralSpaces(newUniqueTitle);

		if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle)))
			return { "error", "error" };

		const BotDatabase::Group& group = botDatabase.GetGroups().at(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle));

		botDatabase.UpdateGroup(BotDatabase::Group{
		.id = group.id,
		.title = group.title,
		.uniqueTitle = newUniqueTitle,
		.type = group.type,
		.isBotAdmin = group.isBotAdmin,
		.isBotActive = group.isBotActive
			});

		return { commandParameters, commandParameters };

	}
	else
		return { "test", "test" };
}

OnEventResult BotController::OnBan(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
	{
		if(botDatabase.IsAdmin(message->from->id))
		{
			Log(LogSource::Database, LogType::Event, "The user is the bot administrator");

			if (message->replyToMessage)
			{
				string banDuration = message->text.substr(message->text.size() == 4 ? 4 : 5);

				if (banDuration.empty())
				{
					banDuration = '0';
				}

				auto banDurationInHours = std::chrono::hours(stoi(banDuration));
				auto banDurationInMinutes = std::chrono::minutes(int((stod(banDuration) - banDurationInHours.count()) * 100));

				auto untilTimePoint = chrono::system_clock::now() + banDurationInHours + banDurationInMinutes;

				time_t untilTimestamp = chrono::system_clock::to_time_t(untilTimePoint);


				if (bot.getApi().banChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, static_cast<int32_t>(untilTimestamp)))
				{
					Log(LogSource::Bot, LogType::Event, "User banned for " + to_string(banDurationInHours.count()) + " hours " + to_string(banDurationInMinutes.count()) + " minutes");
					bot.getApi().sendMessage(message->chat->id, "The user has been banned for " + to_string(banDurationInHours.count()) + " hours  " + to_string(banDurationInMinutes.count()) + " minutes");
				}
			}
		}
	}
	return { "test", "test" };

}

OnEventResult BotController::OnUnban(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
	{
		if(botDatabase.IsAdmin(message->from->id))
		{
			Log(LogSource::Database, LogType::Event, "The user is the bot administrator");

			if (message->replyToMessage)
			{

				if (bot.getApi().unbanChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, true))
				{
					Log(LogSource::Bot, LogType::Event, "User unbanned");
					bot.getApi().sendMessage(message->chat->id, "The user has been unbanned");
				}
			}
		}
	}
	return { "test", "test" };

}

OnEventResult BotController::OnMute(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
	{
		if(botDatabase.IsAdmin(message->from->id))
		{
			Log(LogSource::Database, LogType::Event, "The user is the bot administrator");

			if (message->replyToMessage)
			{
				string banDuration = message->text.substr(message->text.size() == 4 ? 4 : 5);

				if (banDuration.empty())
				{
					banDuration = '0';
				}

				auto banDurationInHours = std::chrono::hours(stoi(banDuration));
				auto banDurationInMinutes = std::chrono::minutes(int((stod(banDuration) - banDurationInHours.count()) * 100));

				auto untilTimePoint = chrono::system_clock::now() + banDurationInHours + banDurationInMinutes;

				time_t untilTimestamp = chrono::system_clock::to_time_t(untilTimePoint);

				ChatPermissions::Ptr permissions{ new ChatPermissions };

				permissions->canSendMessages		= false;
				permissions->canSendOtherMessages	= false;
				permissions->canSendAudios			= false;
				permissions->canSendDocuments		= false;
				permissions->canSendPhotos			= false;
				permissions->canSendPolls			= false;
				permissions->canSendVideoNotes		= false;
				permissions->canSendVideos			= false;
				permissions->canSendVoiceNotes		= false;
				permissions->canAddWebPagePreviews	= false;

				if (bot.getApi().restrictChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, permissions, untilTimestamp))
				{

					Log(LogSource::Bot, LogType::Event, "User mutted for " + to_string(banDurationInHours.count()) + " hours " + to_string(banDurationInMinutes.count()) + " minutes");
					bot.getApi().sendMessage(message->chat->id, "The user has been muted for " + to_string(banDurationInHours.count()) + " hours  " + to_string(banDurationInMinutes.count()) + " minutes");
				}
			}
		}
	}
	return { "test", "test" };

}

OnEventResult BotController::OnUnmute(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
	{
		if(botDatabase.IsAdmin(message->from->id))
		{
			Log(LogSource::Database, LogType::Event, "The user is the bot administrator");

			if (message->replyToMessage)
			{
				ChatPermissions::Ptr permissions{ new ChatPermissions };

				permissions->canSendMessages		= true;
				permissions->canSendOtherMessages	= true;
				permissions->canSendAudios			= true;
				permissions->canSendDocuments		= true;
				permissions->canSendPhotos			= true;
				permissions->canSendPolls			= true;
				permissions->canSendVideoNotes		= true;
				permissions->canSendVideos			= true;
				permissions->canSendVoiceNotes		= true;
				permissions->canAddWebPagePreviews	= true;

				if (bot.getApi().restrictChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, permissions))
				{

					Log(LogSource::Bot, LogType::Event, "User unmutted");
					bot.getApi().sendMessage(message->chat->id, "The user is unmuted");
				}
			}
		}
	}
	return { "test", "test" };

}

OnEventResult BotController::OnNonCommand(Message::Ptr message)
{
	const auto dataBot			= bot.getApi().getMe();
	const string messageText	= message->text;
	const Chat::Type chatType	= message->chat->type;
	const int64_t chatId		= message->chat->id;

	if (isSystemMessage(message))
	{
		//cout << "delete" << '\n';
		//bot.getApi().deleteMessage(chatId, message->messageId);
	}
	else
	{
		switch (chatType)
		{
		case Chat::Type::Private:
			Log(LogSource::Bot, LogType::Event, "Private user " + to_string(message->from->id) + " wrote " + messageText);
			break;
		case Chat::Type::Group:
			Log(LogSource::Bot, LogType::Event, "In group " + to_string(chatId) + ", user " + to_string(message->from->id) + " wrote " + messageText);
			break;
		case Chat::Type::Supergroup:

			if (bot.getToken() == "8231301649:AAEtgMiY1ukuwycs5RWus5IDVfQbrHv7BKo")
			{
				bot.getApi().sendMessage(message->chat->id, message->text);
			}
			Log(LogSource::Bot, LogType::Event, "In supergroup " + to_string(chatId) + ", user " + to_string(message->from->id) + " wrote " + messageText);
			break;
		case Chat::Type::Channel:
			Log(LogSource::Bot, LogType::Event, "In channel " + to_string(chatId) + ", user " + to_string(message->from->id) + " wrote " + messageText);
			break;
		default:
			throw "Unknown chat type";
			break;
		}
	}
	return { "test", "test" };

}

OnEventResult BotController::onMyChatMember(ChatMemberUpdated::Ptr update)
{
	const bool isContains = botDatabase.GetGroups().contains(update->chat->id);
	const string status = update->newChatMember->status;

	if (!isContains && (status == "member" || status == "administrator"))
	{
		botDatabase.AddGroup(BotDatabase::Group{
		.id				= update->chat->id,
		.title			= update->chat->title,
		.uniqueTitle	= RandomNumberGenerator(32),
		.type			= update->chat->type,
		.isBotAdmin		= status == "administrator",
		.isBotActive	= false
			});
	}
	else if (isContains && (status == "member" || status == "administrator"))
	{
		botDatabase.UpdateGroup(BotDatabase::Group{
		.id				= update->chat->id,
		.title			= update->chat->title,
		.uniqueTitle	= (*botDatabase.GetGroups().find(update->chat->id)).second.uniqueTitle,
		.type			= update->chat->type,
		.isBotAdmin		= status == "administrator",
		.isBotActive	= (*botDatabase.GetGroups().find(update->chat->id)).second.isBotActive
			});
	}
	else if (isContains && (status == "left" || status == "kicked"))
	{
		botDatabase.DeleteGroup(update->chat->id);
	}
	return { "test", "test" };

}

bool BotController::isSystemMessage(const Message::Ptr& message)
{
	return (
			!message->newChatMembers.empty()	
		||	message->leftChatMember != nullptr
		||	!message->newChatTitle.empty()
		||	!message->newChatPhoto.empty()
		||	message->deleteChatPhoto	
		||	message->groupChatCreated	
		||	message->supergroupChatCreated
		||	message->channelChatCreated	
		||	message->migrateToChatId != 0
		||	message->migrateFromChatId != 0
		||	message->pinnedMessage != nullptr			
		);
}


string BotController::CleaningUpLateralSpaces(const string_view text)
{
	const size_t l = text.find_first_not_of(' ');

	if (l == string::npos)
		return {};

	const size_t r = text.find_last_not_of(' ');

	return string(text.substr(l, r - l + 1));
}

string BotController::RandomNumberGenerator(const size_t length)
{
	string number;

	for (size_t a = 0; a < length; ++a)
	{
		number += static_cast<char>(uniform_dist(dre));
	}

	return number;
}
