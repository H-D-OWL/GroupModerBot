#include "BotController.h"

BotController::BotController(BotDatabase& botDatabase, Bot& bot) : botDatabase(botDatabase), bot(bot)
{
	if(!botDatabase.GetNumberAdmins())
	{
		confirmationCode.clear();

		confirmationCode = RandomNumberGenerator(64);

		Log(LogSource::Program, LogType::Event, "confirmation code: " + confirmationCode);
	}

	bot.getEvents().onCommand		("start",	[this](Message::Ptr message)			{ SafeExecute(message,	[&](){ OnStart(message); }); });
	bot.getEvents().onCommand		("botActive", [this](Message::Ptr message)			{ SafeExecute(message,	[&](){ OnBotActive(message); }); });
	bot.getEvents().onCommand		("botDeactive", [this](Message::Ptr message)		{ SafeExecute(message,	[&](){ OnBotDeactive(message); }); });
	bot.getEvents().onCommand		("groups",	[this](Message::Ptr message)			{ SafeExecute(message,	[&](){ OnGroups(message); }); });
	bot.getEvents().onCommand		("ban",		[this](Message::Ptr message)			{ SafeExecute(message,	[&](){ OnBan(message); }); });
	bot.getEvents().onCommand		("unban",	[this](Message::Ptr message)			{ SafeExecute(message,	[&](){ OnUnban(message); }); });
	bot.getEvents().onCommand		("mute",	[this](Message::Ptr message)			{ SafeExecute(message,	[&](){ OnMute(message); }); });
	bot.getEvents().onCommand		("unmute",	[this](Message::Ptr message)			{ SafeExecute(message,	[&](){ OnUnmute(message); }); });
	bot.getEvents().onMyChatMember	(			[this](ChatMemberUpdated::Ptr update)	{ SafeExecute(update,	[&](){ onMyChatMember(update); }); });

}

void BotController::Run()
{
	TgLongPoll longPoll(bot);

	while (true)
		longPoll.start();
}

void BotController::OnStart(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

		if (confirmationCode != "ERROR" && code == confirmationCode)
		{
			botDatabase.AddAdmin(BotDatabase::Admin{
		.id = message->from->id,
		.firstName = message->from->firstName,
		.lastName = message->from->lastName,
		.username = message->from->username,
		.isBot = message->from->isBot,
		.isPremium = message->from->isPremium,
		.isBotOwner = true
				});

			confirmationCode = "ERROR";

			Log(LogSource::Bot, LogType::Event, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " entered the correct confirmation code and became a moderator");
			bot.getApi().sendMessage(message->chat->id, "You have become a moderator");

		}
		else if (confirmationCode != "ERROR" && !code.empty())
		{
			Log(LogSource::Bot, LogType::Error, "user: " + to_string(message->from->id) + " entered an incorrect confirmation code and did not become a moderator");

			bot.getApi().sendMessage(message->chat->id, "The confirmation code is incorrect");

		}
	}
	else
	{
		throw runtime_error{ "start" };

	}
}

void BotController::OnBotActive(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
		botDatabase.UpdateGroup(BotDatabase::Group{
		.id = message->chat->id,
		.title = message->chat->title,
		.type = message->chat->type,
		.isBotAdmin = (*botDatabase.GetGroups().find(message->chat->id)).second.isBotAdmin,
		.isBotActive = true
			});
}

void BotController::OnBotDeactive(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
		botDatabase.UpdateGroup(BotDatabase::Group{
		.id = message->chat->id,
		.title = message->chat->title,
		.type = message->chat->type,
		.isBotAdmin = (*botDatabase.GetGroups().find(message->chat->id)).second.isBotAdmin,
		.isBotActive = false
			});
}

void BotController::OnGroups(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		string sendMessageText{};

		size_t number{ 1 };

		for (const auto& [id, group] : botDatabase.GetGroups())
		{
			sendMessageText += to_string(number);
			sendMessageText += ". ";
			sendMessageText += group.title + ":\n    IsBotAdmin: " + (group.isBotAdmin ? "Yes" : "No") + "\n    " + "IsBotActive: " + (group.isBotActive ? "Yes" : "No");
			++number;
		}

		if (number == 1)
		{
			sendMessageText = "Групп нет";
		}

		Log(LogSource::Bot, LogType::Event, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates");

		bot.getApi().sendMessage(message->chat->id, sendMessageText);
	}

}

void BotController::OnBan(Message::Ptr message)
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


				if (bot.getApi().banChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, untilTimestamp))
				{
					Log(LogSource::Bot, LogType::Event, "User banned for " + to_string(banDurationInHours.count()) + " hours " + to_string(banDurationInMinutes.count()) + " minutes");
					bot.getApi().sendMessage(message->chat->id, "The user has been banned for " + to_string(banDurationInHours.count()) + " hours  " + to_string(banDurationInMinutes.count()) + " minutes");
				}
			}
		}
	}

}

void BotController::OnUnban(Message::Ptr message)
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

}

void BotController::OnMute(Message::Ptr message)
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

				if (bot.getApi().restrictChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, permissions, untilTimestamp))
				{

					Log(LogSource::Bot, LogType::Event, "User mutted for " + to_string(banDurationInHours.count()) + " hours " + to_string(banDurationInMinutes.count()) + " minutes");
					bot.getApi().sendMessage(message->chat->id, "The user has been muted for " + to_string(banDurationInHours.count()) + " hours  " + to_string(banDurationInMinutes.count()) + " minutes");
				}
			}
		}
	}
}

void BotController::OnUnmute(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
	{
		if(botDatabase.IsAdmin(message->from->id))
		{
			Log(LogSource::Database, LogType::Event, "The user is the bot administrator");

			if (message->replyToMessage)
			{
				ChatPermissions::Ptr permissions{ new ChatPermissions };

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

				if (bot.getApi().restrictChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, permissions))
				{

					Log(LogSource::Bot, LogType::Event, "User unmutted");
					bot.getApi().sendMessage(message->chat->id, "The user is unmuted");
				}
			}
		}
	}

}

void BotController::OnNonCommand(Message::Ptr message)
{
	const auto dataBot = bot.getApi().getMe();
	const string messageText = message->text;
	const Chat::Type chatType = message->chat->type;
	const int64_t chatId = message->chat->id;

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
}

void BotController::onMyChatMember(ChatMemberUpdated::Ptr update)
{
	const bool isContains = botDatabase.GetGroups().contains(update->chat->id);
	const string status = update->newChatMember->status;

	if (!isContains && (status == "member" || status == "administrator"))
	{
		botDatabase.AddGroup(BotDatabase::Group{
		.id = update->chat->id,
		.title = update->chat->title,
		.type = update->chat->type,
		.isBotAdmin = status == "administrator",
		.isBotActive = false
			});
	}
	else if (isContains && (status == "member" || status == "administrator"))
	{
		botDatabase.UpdateGroup(BotDatabase::Group{
		.id = update->chat->id,
		.title = update->chat->title,
		.type = update->chat->type,
		.isBotAdmin = status == "administrator",
		.isBotActive = (*botDatabase.GetGroups().find(update->chat->id)).second.isBotActive
			});
	}
	else if (isContains && (status == "left" || status == "kicked"))
	{
		botDatabase.DeleteGroup(update->chat->id);
	}
}

bool BotController::isSystemMessage(const Message::Ptr& message)
{
	return (
		!message->newChatMembers.empty()	||
		message->leftChatMember != nullptr	||
		!message->newChatTitle.empty()		||
		!message->newChatPhoto.empty()		||
		message->deleteChatPhoto			||
		message->groupChatCreated			||
		message->supergroupChatCreated		||
		message->channelChatCreated			||
		message->migrateToChatId != 0		||
		message->migrateFromChatId != 0		||
		message->pinnedMessage != nullptr			
		);
}

string BotController::RandomNumberGenerator(const size_t numberOfNumbers)
{
	string number;

	for (auto a = 0; a < numberOfNumbers; ++a)
	{
		number += uniform_dist(dre);
	}

	return number;
}
