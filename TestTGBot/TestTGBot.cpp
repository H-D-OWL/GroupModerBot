#include <iostream>
#include <stdio.h>
#include <tgbot/tgbot.h>
#include <string>
#include <vector>
#include <fstream>
#include <SQLiteCpp/SQLiteCpp.h>
#include <memory>
#include <unordered_map>
#include <random>
#include "logging.h"
#include <chrono>
#include "BotDatabase.h"
#include "BotController.h"

/*
	1. Получение токина и пути к базе данных из файла DataForBot.txt.
	2. Проверка пути к базе данных и наличия в ней всех необходимых таблиц и столбчов, проверку токена.
	3. Выдача кода подтверждения для подтверждения прав первого администратора бота.
	4. Настройка бота через личную группу в телеграме.
	5. Использование команд в группе (бан, мьют, отключение возможности песать и другие).
	database

*/






using namespace std;
using namespace TgBot;
using namespace logging;

void SendManagerUI(const Bot& bot, const int64_t chatId)
{
	InlineKeyboardMarkup::Ptr keybord(new InlineKeyboardMarkup);
	InlineKeyboardButton::Ptr buttonAboutBot(new InlineKeyboardButton), buttonChats(new InlineKeyboardButton);

	buttonAboutBot->text = "О боте";
	buttonAboutBot->callbackData = "AboutBot";

	buttonChats->text = "Чаты";
	buttonChats->callbackData = "Chats";


	keybord->inlineKeyboard.push_back({ buttonChats ,buttonAboutBot });

	bot.getApi().sendMessage(chatId, "Вы стали модератором.", false, 0, keybord);
}

string RandomNumberGenerator(const size_t numberOfNumbers)
{
	random_device rd;
	default_random_engine dre(rd());
	uniform_int_distribution<int> uniform_dist('0', '9');
	string number;

	for (auto i = 0; i < numberOfNumbers; ++i)
	{
		number += uniform_dist(dre);
	}

	return number;
}

bool isSystemMessage(const TgBot::Message::Ptr& message)
{
	return (
		!message->newChatMembers.empty() ||		// Новый участник
		message->leftChatMember != nullptr ||		// Участник покинул чат
		!message->newChatTitle.empty() ||		// Изменено название
		!message->newChatPhoto.empty() ||		// Изменено фото чата
		message->deleteChatPhoto ||		// Фото удалено
		message->groupChatCreated ||		// Группа создана
		message->supergroupChatCreated ||		// Супергруппа создана
		message->channelChatCreated ||		// Канал создан
		message->migrateToChatId != 0 ||		// Миграция в супергруппу
		message->migrateFromChatId != 0 ||		// Миграция из группы
		message->pinnedMessage != nullptr			// Закреплено сообщение
		);
}

struct Table
{
	const string tableName{};
	const vector<string> columnNames{};
};

int main()
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	// ============================================================
	//Работа с файлом
	// ============================================================


	ifstream fileDataForBot("DataForBot.txt", ios_base::in);

	if (!fileDataForBot.is_open())
	{
		Log({ LogPrefix::Program, LogPrefix::Error }, "file \"DataForBot.txt\" not found");
		return 1;
	}

	Log({ LogPrefix::Program, LogPrefix::Event }, "file \"DataForBot.txt\" found");

	string dbPath{ "ERROR" }, botToken{ "ERROR" };

	while(fileDataForBot.good())
	{
		string fileLine{};
		getline(fileDataForBot, fileLine);

		if (const auto off = fileLine.find("DBPath="); off != string::npos)
		{
			dbPath = fileLine.substr(off + 9);
		}
		else if (const auto off = fileLine.find("BotToken="); off != string::npos)
		{
			botToken = fileLine.substr(off + 9);
		}
	}

	fileDataForBot.close();

	// ============================================================
	//Работа с ДБ
	// ============================================================


	BotDatabase botDatabase;
	string confirmationCode{ "ERROR" };

	try
	{
		botDatabase.Open(dbPath);

		Log({ LogPrefix::Database, LogPrefix::Event }, "batabase: " + dbPath.substr(dbPath.rfind('\\') + 1) + " found");

		botDatabase.CheckStructure();

		botDatabase.CacheReload();

		if(botDatabase.GetAdminIds().empty())
		{
			confirmationCode.clear();

			confirmationCode = RandomNumberGenerator(64);

			Log({ LogPrefix::Program, LogPrefix::Event }, "confirmation code: " + confirmationCode);
		}
	}
	catch (exception& e)
	{
		Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
		return 2;
	}

	// ============================================================
	//Работа с ботом
	// ============================================================
	//botToken = "8231301649:AAEtgMiY1ukuwycs5RWus5IDVfQbrHv7BKo";
	Bot bot(botToken);

	if (bot.getToken().empty())
	{
		Log({ LogPrefix::Bot, LogPrefix::Error }, "botToken is invalid");
		return 3;
	}

	Log({ LogPrefix::Bot, LogPrefix::Event }, "botToken is valid");

	// ============================================================
	//Управление ботом
	// ============================================================

	BotController controller{ dbPath, botToken, botDatabase };

	/*
		/start
		/addGroup
		/deleteGroup
		/groups
		/ban
		/unban
		/mute
		/unmute
		/help
		/about

	*/

	bot.getEvents().onCommand("start", [&bot, &botDatabase, &confirmationCode](Message::Ptr message)
		{
			Log({ LogPrefix::Database, LogPrefix::Error }, message->chat->firstName + ' ' + message->chat->lastName + ' ' + message->chat->title + ' ' + message->chat->username + ' ' + message->chat->bio);

			if (message->chat->type == Chat::Type::Private)
			{
				string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

				if (confirmationCode != "ERROR" && code == confirmationCode)
				{
					try
					{
						botDatabase.SetAdmin(message->from->id, message->from->firstName);

						confirmationCode = "ERROR";

						Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " entered the correct confirmation code and became a moderator");
						bot.getApi().sendMessage(message->chat->id, "Вы стали модератором.");

						//SendManagerUI(bot, message->chat->id);
					}
					catch (exception& e)
					{
						Log({ LogPrefix::Database, LogPrefix::Error }, e.what());

						bot.getApi().sendMessage(message->chat->id, "Произошла техническая ошибка. Вы не стали модератором.");
					}
				}
				else if (confirmationCode != "ERROR" && !code.empty())
				{
					Log({ LogPrefix::Bot, LogPrefix::Error }, "user: " + to_string(message->from->id) + " entered an incorrect confirmation code and did not become a moderator");
					bot.getApi().sendMessage(message->chat->id, "Код подтверждения неверен.");
				}
			}
		});

	string codeForAddingGroup{ "ERROR" };

	bot.getEvents().onCommand("addGroup", [&bot, &botDatabase, &codeForAddingGroup](Message::Ptr message)
		{
			try
			{
				if (message->chat->type == Chat::Type::Private)
				{
					if(botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						Log({ LogPrefix::Database, LogPrefix::Event }, "The user is the bot administrator");

						codeForAddingGroup.clear();

						codeForAddingGroup = RandomNumberGenerator(32);

						bot.getApi().sendMessage(message->chat->id, "Напишите в группе которую хотите добавить это: /addGroup " + codeForAddingGroup);
					}
					else
					{
						Log({ LogPrefix::Database, LogPrefix::Error }, "The user is not the bot administrator");
					}
				}
				else if (message->chat->type == Chat::Type::Group || message->chat->type == Chat::Type::Supergroup)
				{
					string code = message->text.substr(message->text.size() == 9 ? 9 : 10);

					if (codeForAddingGroup != "ERROR" && code == codeForAddingGroup)
					{
						if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
						{
							botDatabase.AddGroup(message->chat->id, message->chat->title, bot.getApi().getChatMember(message->chat->id, bot.getApi().getMe()->id)->status == "administrator");
						}
						else
						{
							Log({ LogPrefix::Database, LogPrefix::Error }, "Group " + message->chat->title + " already added");
						}


						Log({ LogPrefix::Database, LogPrefix::Event }, "The group has been added");

						codeForAddingGroup = "ERROR";
					}
				}
			}
			catch (exception& e)
			{
				Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
			}
		});

	bot.getEvents().onCommand("deleteGroup", [&bot, &botDatabase](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				string groupName = message->text.substr(message->text.size() == 12 ? 12 : 13);

				try
				{

					if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						Log({ LogPrefix::Database, LogPrefix::Event }, "The user is the bot administrator");

						if(botDatabase.DeleteGroup(groupName))
						{
							Log({ LogPrefix::Database, LogPrefix::Event }, "group " + groupName + " delete");

							bot.getApi().sendMessage(message->chat->id, "группа " + groupName + " удалена");
						}
						else
						{
							Log({ LogPrefix::Database, LogPrefix::Event }, "group " + groupName + " not delete");

							bot.getApi().sendMessage(message->chat->id, "группа " + groupName + " не удалена");
						}
					}
					else
					{
						Log({ LogPrefix::Database, LogPrefix::Error }, "The user is not the bot administrator");
					}
				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		}
	);

	bot.getEvents().onCommand("groups", [&bot, &botDatabase, &confirmationCode](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				try
				{
					string sendMessageText{};

					//SQLite::Statement query{ *botDatabase.Get(), "SELECT " + Groups.columnNames[1] + ',' + Groups.columnNames[2] + " FROM " + Groups.tableName};
					size_t number{ 1 };

					//while (query.executeStep())
					for (const auto& a : botDatabase.GetGroupNames())
					{
						sendMessageText += to_string(number);
						sendMessageText += ". ";
						sendMessageText += a;
						//sendMessageText += (query.getColumn(1).getInt() == 1 ? " - Бот администротор" : " - Бот не администротор");
						++number;
					}

					if (number == 1)
					{
						sendMessageText = "Групп нет";
					}

					Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates");

					bot.getApi().sendMessage(message->chat->id, sendMessageText);
				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		});

	bot.getEvents().onCommand("ban", [&bot, &botDatabase](Message::Ptr message)
		{
			if (message->chat->type != Chat::Type::Private)
			{
				try
				{
					if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						Log({ LogPrefix::Database, LogPrefix::Event }, "The user is the bot administrator");

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


							if(bot.getApi().banChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, untilTimestamp))
							{
								Log({ LogPrefix::Bot, LogPrefix::Event }, "User banned for " + to_string(banDurationInHours.count()) + " hours " + to_string(banDurationInMinutes.count()) + " minutes");
								bot.getApi().sendMessage(message->chat->id, "Пользователь забанен на " + to_string(banDurationInHours.count()) + " часов " + to_string(banDurationInMinutes.count()) + " минут");
							}
						}
					}

					//Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates");

				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		});

	bot.getEvents().onCommand("unban", [&bot, &botDatabase](Message::Ptr message)
		{
			if (message->chat->type != Chat::Type::Private)
			{
				try
				{
					if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						Log({ LogPrefix::Database, LogPrefix::Event }, "The user is the bot administrator");

						if (message->replyToMessage)
						{

							if(bot.getApi().unbanChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, true))
							{
								Log({ LogPrefix::Bot, LogPrefix::Event }, "User unbanned");
								bot.getApi().sendMessage(message->chat->id, "Пользователь разбанен");
							}
						}
					}

					//Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates");

				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		});

	bot.getEvents().onCommand("mute", [&bot, &botDatabase](Message::Ptr message)
		{
			if (message->chat->type != Chat::Type::Private)
			{
				try
				{
					if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						Log({ LogPrefix::Database, LogPrefix::Event }, "The user is the bot administrator");

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

							ChatPermissions::Ptr permissions{new ChatPermissions};

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

							if(bot.getApi().restrictChatMember(message->replyToMessage->chat->id, message->replyToMessage->from->id, permissions, untilTimestamp))
							{

								Log({ LogPrefix::Bot, LogPrefix::Event }, "User mutted for " + to_string(banDurationInHours.count()) + " hours " + to_string(banDurationInMinutes.count()) + " minutes");
								bot.getApi().sendMessage(message->chat->id, "Пользователь замьютен на " + to_string(banDurationInHours.count()) + " часов " + to_string(banDurationInMinutes.count()) + " минут");
							}
						}
					}

					//Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates");

				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		});

	bot.getEvents().onCommand("unmute", [&bot, &botDatabase](Message::Ptr message)
		{
			if (message->chat->type != Chat::Type::Private)
			{
				try
				{
					if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						Log({ LogPrefix::Database, LogPrefix::Event }, "The user is the bot administrator");

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

								Log({ LogPrefix::Bot, LogPrefix::Event }, "User unmutted");
								bot.getApi().sendMessage(message->chat->id, "Пользователь размьютен");
							}
						}
					}

					//Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " looked at the groups in which the bot operates");

				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		});

	bot.getEvents().onCommand("help", [&bot, &botDatabase](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				try
				{
					if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						bot.getApi().sendMessage(message->chat->id, "");

					}
				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		});

	bot.getEvents().onCommand("about", [&bot, &botDatabase](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				try
				{
					if (botDatabase.GetAdminIds().find(message->from->id) != botDatabase.GetAdminIds().cend())
					{
						bot.getApi().sendMessage(message->chat->id, "Здравствуйте. Hello.");
					}
				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}
			}
		});

	bot.getEvents().onCallbackQuery([&bot](CallbackQuery::Ptr query)
		{
			if (query->data == "AboutBot")
			{

			}
			Log({ LogPrefix::Bot, LogPrefix::Event }, query->inlineMessageId + ' ' + query->id + ' ' + query->data + ' ' + query->message->from->lastName + ' ' + to_string(query->from->id));
			bot.getApi().sendMessage(query->from->id, " Нажата кнопка: " + query->data);
			//bot.getApi().editMessageText("Сообщение изменено", query->from->id, query->message->messageId);
		});

	bot.getEvents().onMyChatMember([&bot, &botDatabase](ChatMemberUpdated::Ptr upd)
		{
			if (upd->chat->type != Chat::Type::Private)
			{
				try
				{
					if (botDatabase.GetAdminIds().find(upd->from->id) != botDatabase.GetAdminIds().cend())
					{
						cout << "my_chat_member: chat=" << upd->chat->id << " old=" << upd->oldChatMember->status << " new=" << upd->newChatMember->status << endl;

						//botDatabase->exec("UPDATE " + Groups.tableName + " SET " + Groups.columnNames[1] + "='" + upd->chat->title + "'," + Groups.columnNames[2] + "='" + to_string(bot.getApi().getChatMember(upd->chat->id, bot.getApi().getMe()->id)->status == "administrator") + "' WHERE " + Groups.columnNames[0] + '=' + to_string(upd->chat->id));
						botDatabase.UpdateGroup(upd->chat->id, upd->chat->title, bot.getApi().getChatMember(upd->chat->id, bot.getApi().getMe()->id)->status == "administrator");
					}

				}
				catch (exception& e)
				{
					Log({ LogPrefix::Database, LogPrefix::Error }, e.what());
				}


			}


			/*if (upd->newChatMember->status == "creator" || upd->newChatMember->status == "administrator")
			{
				auto member = bot.getApi().getChatMember(upd->chat->id, bot.getApi().getMe()->id);
				auto adminPtr = dynamic_pointer_cast<ChatMemberAdministrator>(member);

				if (adminPtr)
				{
					//adminPtr->
					// теперь доступ к полям администратора
					bool canDelete = adminPtr->canDeleteMessages;
					cout << canDelete << "\n";
					// ...
				}
			}
			else if (upd->newChatMember->status == "member" || upd->newChatMember->status == "restricted")
			{
				cout << upd->newChatMember->status << "\n";
			}
			else if (upd->newChatMember->status == "left" || upd->newChatMember->status == "kicked")
			{
				cout << upd->newChatMember->status << "\n";
			}*/


		});

	//int64_t chatiId = 0, mId = 0;
	//chrono::steady_clock::time_point start{};
	//bool b = false;

	bot.getEvents().onNonCommandMessage([&bot/*, &chatiId, &mId, &start, &b*/](Message::Ptr message)
		{
			try
			{
				const auto dataBot = bot.getApi().getMe();
				const string messageText = message->text;
				const Chat::Type chatType = message->chat->type;
				const int64_t chatId = message->chat->id;

				//cout << bot.getApi().getChatMember(message->chat->id, bot.getApi().getMe()->id)->status << '\n';



				//cout << "стоп" << "\n";
				//start = chrono::steady_clock::now();
				//
				//chatiId = message->chat->id;
				//mId = message->messageId;
				//b = false;
				////this_thread::sleep_for(10s);
				//Sleep(1000000);
				//if (!message->photo.empty())
				//{
				//	bot.getApi().sendPhoto(chatId, message->photo[0]->fileId);
				//}
				//bot.getApi().sendMessage(chatId, "Your message contained: \"" + messageText + "\"");

				if (isSystemMessage(message))
				{
					//cout << "delete" << '\n';
					//bot.getApi().deleteMessage(chatId, message->messageId);

					//if (message->newChatMembers.size() > 0) 
					//{
					//	bot.getApi().deleteMessage(chatId, message->messageId);

					//	// Системное: новые участники
					//}
					//else if (message->leftChatMember) 
					//{
					//	bot.getApi().deleteMessage(chatId, message->messageId);

					//	// Системное: участник покинул группу
					//}
					//else if (!message->newChatTitle.empty()) 
					//{
					//	bot.getApi().deleteMessage(chatId, message->messageId);

					//	// Системное: изменено название группы
					//}
					//else if (message->pinnedMessage) 
					//{
					//	bot.getApi().deleteMessage(chatId, message->messageId);

					//	// Системное: закреплено сообщение
					//}
					//else if (message->deleteChatPhoto) 
					//{
					//	bot.getApi().deleteMessage(chatId, message->messageId);

					//	// Системное: удалено фото группы
					//}
					//else if (!message->newChatPhoto.empty())
					//{
					//	bot.getApi().deleteMessage(chatId, message->messageId);

					//	// Системное: удалено фото группы
					//}
				}
				else
				{
					switch (chatType)
					{
					case Chat::Type::Private:
						Log({ LogPrefix::Bot, LogPrefix::Event }, "Private user " + to_string(message->from->id) + " wrote " + messageText);
						break;
					case Chat::Type::Group:
						Log({ LogPrefix::Bot, LogPrefix::Event }, "In group " + to_string(chatId) + ", user " + to_string(message->from->id) + " wrote " + messageText);
						break;
					case Chat::Type::Supergroup:

						if (bot.getToken() == "8231301649:AAEtgMiY1ukuwycs5RWus5IDVfQbrHv7BKo")
						{
							bot.getApi().sendMessage(message->chat->id, message->text);
						}
						Log({ LogPrefix::Bot, LogPrefix::Event }, "In supergroup " + to_string(chatId) + ", user " + to_string(message->from->id) + " wrote " + messageText);
						break;
					case Chat::Type::Channel:
						Log({ LogPrefix::Bot, LogPrefix::Event }, "In channel " + to_string(chatId) + ", user " + to_string(message->from->id) + " wrote " + messageText);
						break;
					default:
						throw "Unknown chat type";
						break;
					}
				}
			}
			catch (const char* errorMessage)
			{
				cout << errorMessage << "\n";
			}
			catch (...)
			{
				cout << "Unknown error" << "\n";
			}

		});

	bot.getEvents().onAnyMessage([&bot](Message::Ptr message)
		{
			//cout << "Action 3" << "\n";

			//try
			//{
			//	cout << "Action" << "\n";
			//	const auto dataBot = bot.getApi().getMe();

			//	const Chat::Type chatType = message->chat->type;

			//	if (message->leftChatMember)
			//		if (message->leftChatMember->id == dataBot->id)
			//			cout << "Bot removed from chat " << message->chat->id << endl;

			//	if (!message->newChatMembers.empty())
			//	{
			//		for (const auto& member : message->newChatMembers)
			//		{
			//			if (member->id == dataBot->id)
			//			{
			//				// 🎯 Бота только что добавили в группу
			//				cout << "Bot added to chat: "
			//					<< message->chat->id << endl;
			//			}
			//		}
			//	}

			//	switch (chatType)
			//	{
			//	case Chat::Type::Private:
			//		cout << "Private user " << message->from->id << " wrote " << message->text << "\n";

			//		break;
			//	case Chat::Type::Group:
			//		cout << "Group " << message->chat->id << ", user " << message->from->id << " wrote " << message->text << "\n";

			//		break;
			//	case Chat::Type::Supergroup:
			//		cout << "Supergroup " << message->chat->id << ", user " << message->from->id << " wrote " << message->text << "\n";

			//		break;
			//	case Chat::Type::Channel:
			//		cout << "Channel " << message->chat->id << ", user " << message->from->id << " wrote " << message->text << "\n";

			//		break;
			//	default:
			//		throw "Unknown chat type";
			//		break;
			//	}
			//}
			//catch (const char* errorMessage)
			//{
			//	cout << errorMessage << "\n";
			//}
			//catch (...)
			//{
			//	cout << "Error" << "\n";
			//}
		});

	// ============================================================
	//Цикл работы бота
	// ============================================================

	try
	{
		Log({ LogPrefix::Bot, LogPrefix::Event }, "bot: " + bot.getApi().getMe()->username + " launched");

		TgLongPoll longPoll(bot/*, 100, 5*/);

		while (true)
		{
			//cout << 123456789 << '\n';
			//
			//if (chatiId != 0 && mId != 0 && !b)
			//{
			//	chrono::steady_clock::time_point finish = chrono::steady_clock::now();
			//	cout << chrono::duration_cast<chrono::seconds>(finish - start).count() << '\n';
			//	if (chrono::duration_cast<chrono::seconds>(finish - start).count() >= 30)
			//	{
			//		bot.getApi().deleteMessage(chatiId, mId);
			//		b = true;
			//	}
			//}

			longPoll.start();
		}
	}
	catch (TgException& e)
	{
		Log({ LogPrefix::Bot, LogPrefix::Error }, e.what());
	}

	return 0;
}


