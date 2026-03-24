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

	bot.getEvents().onCommand("admins",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "admins"),				[&](){ return OnAdmins(message); }); });
	bot.getEvents().onCommand("addAdmin",				[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "addAdmin"),			[&](){ return OnAddAdmin(message); }); });
	bot.getEvents().onCommand("removeAdmin",			[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "removeAdmin"),			[&](){ return OnRemoveAdmin(message); }); });

	//bot.getEvents().onCommand("ban",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "ban"),					[&](){ return OnBan(message); }); });
	//bot.getEvents().onCommand("unban",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "unban"),				[&](){ return OnUnban(message); }); });
	//bot.getEvents().onCommand("mute",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "mute"),				[&](){ return OnMute(message); }); });
	//bot.getEvents().onCommand("unmute",					[this](Message::Ptr message)			{ SafeExecute(ContextLog::ToContextLog(message, "unmute"),				[&](){ return OnUnmute(message); }); });
	
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
		if (botDatabase.IsOwner(message->from->id))
		{
			return { "call from owner", "Template response for the owner (debug)" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { "сall from admin", "Template response for the admin (debug)" };
		}
		else if (botDatabase.GetNumberAdmins() == 0)
		{
			return { "call by possible owner", "Hello. I'm " + bot.getApi().getMe()->username + ". I don't have an owner yet. To become one, enter the /addBotAdmin command and the confirmation code from the console" };
		}
		else
		{
			return { "сall from not admin", "Hello. I'm " + bot.getApi().getMe()->username + ". To become a bot admin, type the command /addBotAdmin and the confirmation code that the bot owner will provide you" };
		}
	}
	else
		return { "сall in non-private chat", "" };
}

OnEventResult BotController::OnBotActive(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
	{
		if (botDatabase.IsOwner(message->from->id))
		{
			botDatabase.UpdateGroup(BotDatabase::Group{
			message->chat->id,
			message->chat->title,
			(*botDatabase.GetGroups().find(message->chat->id)).second.uniqueTitle,
			message->chat->type,
			(*botDatabase.GetGroups().find(message->chat->id)).second.isBotAdmin,
			true,
			(*botDatabase.GetGroups().find(message->chat->id)).second.numWarnToMute,
			(*botDatabase.GetGroups().find(message->chat->id)).second.numWarnToBan
				});

			return { "call from owner1", "1" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { "сall from admin2", "2" };
		}
		else
		{
			return { "сall from not admin3", "3" };
		}
	}
	else
		return { "4", "4" };
}

OnEventResult BotController::OnBotDeactive(Message::Ptr message)
{
	if (message->chat->type != Chat::Type::Private)
	{
		if (botDatabase.IsOwner(message->from->id))
		{
			botDatabase.UpdateGroup(BotDatabase::Group{
			message->chat->id,
			message->chat->title,
			(*botDatabase.GetGroups().find(message->chat->id)).second.uniqueTitle,
			message->chat->type,
			(*botDatabase.GetGroups().find(message->chat->id)).second.isBotAdmin,
			false,
			(*botDatabase.GetGroups().find(message->chat->id)).second.numWarnToMute,
			(*botDatabase.GetGroups().find(message->chat->id)).second.numWarnToBan

				});

			return { "call from owner5", "5" };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { "сall from admin6", "6" };
		}
		else
		{
			return { "сall from not admin7", "7" };
		}
	}
	else
		return { "test8", "test8" };
}

OnEventResult BotController::OnGroups(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		if (botDatabase.IsOwner(message->from->id) || botDatabase.IsAdmin(message->from->id))
		{
			string sendMessageText{};

			size_t number{ 1 };

			for (const auto& [id, groupFromCache] : botDatabase.GetGroups())
			{
				sendMessageText += to_string(number);
				sendMessageText += ". ";
				sendMessageText += groupFromCache.title 
					+ " (" 
					+ groupFromCache.uniqueTitle 
					+ ")" 
					+ ":\n    IsBotAdmin: " 
					+ (groupFromCache.isBotAdmin ? "Yes" : "No") 
					+ "\n    " 
					+ "IsBotActive: " 
					+ (groupFromCache.isBotActive ? "Yes" : "No")
					+ "\n    "
					+ "NumWarnToMute: "
					+ to_string(groupFromCache.numWarnToMute)
					+ "\n    "
					+ "NumWarnToBan: "
					+ to_string(groupFromCache.numWarnToBan);
				sendMessageText += '\n';
				++number;
			}

			if (number == 1)
			{
				sendMessageText = "There are no groups";
			}

			return { "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates", sendMessageText };
		}
		else
		{
			return { "сall from not admin9", "9" };
		}
	}
	else
		return { "test10", "test10" };
}

OnEventResult BotController::OnSetGroupUniqueTitle(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		if (botDatabase.IsOwner(message->from->id))
		{
			stringstream commandParameters(message->text.substr("/setGroupUniqueTitle"sv.size()));

			string oldUniqueTitle{}, newUniqueTitle{};

			if (!(commandParameters >> oldUniqueTitle >> newUniqueTitle))
			{
				return { "20", "20" };
			}

			if (newUniqueTitle.length() > 32)
			{
				return { "21", "21" };
			}

			if (!all_of(newUniqueTitle.begin(), newUniqueTitle.end(), [](unsigned char c) { return isalnum(c) || c == '_'; }))
			{
				return { "19", "19" };
			}

			if (!botDatabase.GetGroups().contains(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle)))
				return { "error11", "error11" };

			const BotDatabase::Group& group = botDatabase.GetGroups().at(botDatabase.GroupIdFromUniqueTitle(oldUniqueTitle));

			botDatabase.UpdateGroup(BotDatabase::Group{
			group.id,
			group.title,
			newUniqueTitle,
			group.type,
			group.isBotAdmin,
			group.isBotActive,
			group.numWarnToMute,
			group.numWarnToBan
				});

			return { newUniqueTitle, newUniqueTitle };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { "сall from admin12", "12" };
		}
		else
		{
			return { "сall from not admin13", "13" };
		}
	}
	else
		return { "test14", "test14" };
}

OnEventResult BotController::OnAdmins(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		if (botDatabase.IsOwner(message->from->id) || botDatabase.IsAdmin(message->from->id))
		{
			string sendMessageText{};

			size_t number{ 1 };

			for (const auto& [id, adminFromCache] : botDatabase.GetAdmins())
			{
				// TODO: Debug check. So that it works with debug admins.
				if(id > 100)
				{
					const User::Ptr admin = bot.getApi().getChatMember(id, id)->user;

					botDatabase.UpdateAdmin(BotDatabase::Admin{
					admin->id,
					admin->firstName,
					admin->lastName,
					admin->username,
					admin->isBot,
					admin->isPremium,
					adminFromCache.isBotOwner
						});
				}

				sendMessageText += to_string(number);
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
				sendMessageText = "There are no groups";
			}

			return { "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates", sendMessageText };
		}
		else
		{
			return { "24", "24" };
		}
	}
	else
		return { "23", "23" };
}

OnEventResult BotController::OnAddAdmin(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		stringstream commandParameters(message->text.substr("/addAdmin"sv.size()));

		string adminConfirmationCode{};

		if (!(commandParameters >> adminConfirmationCode))
		{
			return { "25", "25" };
		}

		if (botDatabase.IsOwner(message->from->id))
		{
			confirmationCode.clear();

			confirmationCode = RandomNumberGenerator(32);

			return { "call from owner15", "Confirmation code = " + confirmationCode };
		}
		else if (botDatabase.IsAdmin(message->from->id))
		{
			return { "сall from admin16", "16" };
		}
		else
		{
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

				return { "confirmation code is correct", "You have become a bot admin" };
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
			}
			else if (confirmationCode != "ERROR" && !adminConfirmationCode.empty())
			{
				return { "confirmation code is incorrect", "The confirmation code is incorrect" };
			}
			else
			{
				return { "test17", "test17" };
			}
		}

		return { "test18", "test18" };

	}
	else
		return { "сalling in a non-private chat", "" };
}

OnEventResult BotController::OnRemoveAdmin(Message::Ptr message)
{
	if (message->chat->type == Chat::Type::Private)
	{
		if (botDatabase.IsOwner(message->from->id))
		{
			stringstream commandParameters(message->text.substr("/removeAdmin"sv.size()));

			size_t adminNumber{};

			if (!(commandParameters >> adminNumber))
			{
				return { "26", "26" };
			}

			--adminNumber;

			if (adminNumber >= 0 && adminNumber < botDatabase.GetNumberAdmins())
			{
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
							return { "test28", "test28" };
						}
					}

					++count;
				}

				return { "test29", "test29" };
			}
			else
			{
				return { "test27", "test27" };
			}
		}
		else
		{
			return { "test30", "test30" };
		}
	}
	else
		return { "test22", "test22" };
}

/*
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
*/

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
				//bot.getApi().sendMessage(message->chat->id, message->text);
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
		update->chat->id,
		update->chat->title,
		RandomNumberGenerator(32),
		update->chat->type,
		status == "administrator",
		false,
		3,
		5
			});
	}
	else if (isContains && (status == "member" || status == "administrator"))
	{
		botDatabase.UpdateGroup(BotDatabase::Group{
		update->chat->id,
		update->chat->title,
		(*botDatabase.GetGroups().find(update->chat->id)).second.uniqueTitle,
		update->chat->type,
		status == "administrator",
		(*botDatabase.GetGroups().find(update->chat->id)).second.isBotActive,
		(*botDatabase.GetGroups().find(update->chat->id)).second.numWarnToMute,
		(*botDatabase.GetGroups().find(update->chat->id)).second.numWarnToBan
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
