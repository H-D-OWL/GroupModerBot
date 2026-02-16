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
#include "botControl.h"

/*
	1. Получение токина и пути к базе данных из файла DataForBot.txt.
	2. Проверка пути к базе данных и наличия в ней всех необходимых таблиц и столбчов, проверку токена.
	3. Выдача кода подтверждения для подтверждения прав первого администратора бота.
	4. Настройка бота через личную группу в телеграме.
	5. Использование команд в группе (бан, мьют, отключение возможности песать и другие).


*/






using namespace std;
using namespace TgBot;
using namespace logging;
using namespace botControl;

bool TableHasColumn(const SQLite::Database& dataBase, const string& tableName, const string& columnName)
{
	SQLite::Statement query(dataBase, "PRAGMA table_info(" + tableName + ")");

	while (query.executeStep())
		if (query.getColumn(1).getText() == columnName)
			return true;

	return false;
}

bool isTableEmpty(const SQLite::Database& dataBase, const string& tableName)
{
	SQLite::Statement query(dataBase, "SELECT 1 FROM " + tableName + " LIMIT 1");
	return !query.executeStep();
}

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

int main()
{
	SetConsoleOutputCP(CP_UTF8);

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

	string  pathToDatabase{ "ERROR" }, botToken{ "ERROR" };

	for (int i = 0; fileDataForBot.good() && i < 2; ++i)
	{
		string fileLine{};
		getline(fileDataForBot, fileLine);

		switch (i)
		{
		case 0:
			if (!fileLine.empty())
				pathToDatabase = fileLine;
			break;
		case 1:
			if (!fileLine.empty())
				botToken = fileLine;
			break;
		}
	}

	fileDataForBot.close();

	// ============================================================
	//Работа с ДБ
	// ============================================================

	unique_ptr<SQLite::Database> dataBase;
	string confirmationCode{ "ERROR" };
	const vector<pair<string, const vector<string>>> dataBasesAndColumnsNames{
		{"BotAdministrators", {"AdministratorId", "AdministratorName", "IsOwner"}},
		{"Groups", {"GroupId", "GroupName", "BotIsAdministrator"}},

	};

	try
	{
		dataBase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);

		Log({ LogPrefix::Database, LogPrefix::Event }, "batabase: " + pathToDatabase.substr(pathToDatabase.rfind('\\') + 1) + " found");

		for (const auto& data : dataBasesAndColumnsNames)
		{
			if (dataBase->tableExists(data.first))
			{
				for (const auto& column : data.second)
				{
					if (!TableHasColumn(*dataBase, data.first, column))
					{
						throw SQLite::Exception("table " + data.first + " has no column named " + column);
					}
				}
			}
			else
			{
				throw SQLite::Exception("no such table: " + data.first);
			}
		}

		if (isTableEmpty(*dataBase, dataBasesAndColumnsNames[0].first))
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

	Bot bot(botToken);

	if (bot.getToken().empty())
	{
		Log({ LogPrefix::Bot, LogPrefix::Error }, "botToken is invalid");
		return 3;
	}

	Log({ LogPrefix::Bot, LogPrefix::Event }, "botToken is valid");

	/*//BotCommand::Ptr t(new BotCommand);
	//t->command = "/swap";
	//t->description = "Swap data";
	//cout << bot.getApi().setMyCommands({t}) << '\n';

	//// 1. Создаем команду
	//BotCommand::Ptr swapCommand(new BotCommand);
	//swapCommand->command = "/swap";
	//swapCommand->description = "Swap data";

	//// 2. Создаем вторую команду для примера
	//BotCommand::Ptr startCommand(new BotCommand);
	//startCommand->command = "/start";
	//startCommand->description = "Start the bot";

	//// 3. Помещаем команды в ВЕКТОР, который ожидает метод [citation:4]
	//std::vector<BotCommand::Ptr> commands;
	//commands.push_back(swapCommand);
	//commands.push_back(startCommand);

	//// 4. Вызываем метод и проверяем результат
	//bool success = bot.getApi().setMyCommands(commands);
	//if (success) {
	//	std::cout << "Commands set successfully!" << std::endl;
	//}
	//else {
	//	std::cerr << "Failed to set commands." << std::endl;
	//}

	//try
	//{
	//	auto chat = bot.getApi().getChat(6690609226);
	//	Log({ LogPrefix::Bot, LogPrefix::Event }, chat->username + ' ' + chat->firstName + ' ' + chat->lastName);

	//}
	//catch (const TgBot::TgException& e) {
	//	// Пользователь не найден или бот не имеет доступа

	//}*/

	// ============================================================
	//Управление ботом
	// ============================================================

	/*
		/addGroup
		/groups
	*/


	bot.getEvents().onCommand("start", [&bot, &dataBase, &confirmationCode, &dataBasesAndColumnsNames](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

				if (confirmationCode != "ERROR" && code == confirmationCode)
				{
					try
					{
						SQLite::Statement query{ *dataBase, "INSERT INTO " + dataBasesAndColumnsNames[0].first + " (" + dataBasesAndColumnsNames[0].second[0] + ',' + dataBasesAndColumnsNames[0].second[1] + ',' + dataBasesAndColumnsNames[0].second[2] + ") VALUES(? , ? , ? )" };

						query.bind(1, message->from->id);
						query.bind(2, message->from->firstName);
						query.bind(3, true);
						query.exec();

						confirmationCode = "ERROR";

						Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " entered the correct confirmation code and became a moderator");

						//bot.getApi().sendMessage(message->chat->id, "Вы стали модератором.");
						SendManagerUI(bot, message->chat->id);
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

	/*bot.getEvents().onCommand("commands", [&bot, &dataBase, &confirmationCode, &dataBasesAndColumnsNames](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

				if (confirmationCode != "ERROR" && code == confirmationCode)
				{
					try
					{
						SQLite::Statement query{ *dataBase, "INSERT INTO " + dataBasesAndColumnsNames[0].first + " (" + dataBasesAndColumnsNames[0].second[0] + ',' + dataBasesAndColumnsNames[0].second[1] + ',' + dataBasesAndColumnsNames[0].second[2] + ") VALUES(? , ? , ? )" };

						query.bind(1, message->from->id);
						query.bind(2, message->from->firstName);
						query.bind(3, message->from->lastName);
						query.exec();

						confirmationCode = "ERROR";

						Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " entered the correct confirmation code and became a moderator");

						//bot.getApi().sendMessage(message->chat->id, "Вы стали модератором.");
						SendManagerUI(bot, message->chat->id);
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
		});*/



	string codeForAddingGroup{ "ERROR" };


	bot.getEvents().onCommand("addGroup", [&bot, &dataBase, &codeForAddingGroup, &dataBasesAndColumnsNames](Message::Ptr message)
		{
			try
			{
				if (message->chat->type == Chat::Type::Private)
				{
					cout << message->chat->title << '\n';
					message->chat->title;

					SQLite::Statement query{ *dataBase, "SELECT " + dataBasesAndColumnsNames[0].second[0] + " FROM " + dataBasesAndColumnsNames[0].first + " WHERE " + dataBasesAndColumnsNames[0].second[0] + '=' + to_string(message->from->id) };

					if (query.executeStep())
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


					/*string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

					if (confirmationCode != "ERROR" && code == confirmationCode)
					{
						try
						{
							SQLite::Statement query{ *dataBase, "INSERT INTO " + dataBasesAndColumnsNames[0].first + " (" + dataBasesAndColumnsNames[0].second[0] + ',' + dataBasesAndColumnsNames[0].second[1] + ',' + dataBasesAndColumnsNames[0].second[2] + ") VALUES(? , ? , ? )" };

							query.bind(1, message->from->id);
							query.bind(2, message->from->firstName);
							query.bind(3, message->from->lastName);
							query.exec();

							confirmationCode = "ERROR";

							Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " entered the correct confirmation code and became a moderator");

							//bot.getApi().sendMessage(message->chat->id, "Вы стали модератором.");
							SendManagerUI(bot, message->chat->id);
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
					}*/
				}
				else if (message->chat->type == Chat::Type::Group || message->chat->type == Chat::Type::Supergroup)
				{
					string code = message->text.substr(message->text.size() == 9 ? 9 : 10);

					if (codeForAddingGroup != "ERROR" && code == codeForAddingGroup)
					{
						SQLite::Statement groupAvailabilityQuery{ *dataBase, "SELECT " + dataBasesAndColumnsNames[1].second[0] + " FROM " + dataBasesAndColumnsNames[1].first + " WHERE " + dataBasesAndColumnsNames[1].second[0] + '=' + to_string(message->chat->id) };

						if (!groupAvailabilityQuery.executeStep())
						{
							SQLite::Statement queryToAddGroup{ *dataBase, "INSERT INTO " + dataBasesAndColumnsNames[1].first + " (" + dataBasesAndColumnsNames[1].second[0] + ',' + dataBasesAndColumnsNames[1].second[1] + ',' + dataBasesAndColumnsNames[1].second[2] + ") VALUES(? , ? , ? )" };

							queryToAddGroup.bind(1, message->chat->id);
							queryToAddGroup.bind(2, message->chat->title);
							queryToAddGroup.bind(3, bot.getApi().getChatMember(message->chat->id, bot.getApi().getMe()->id)->status == "administrator");
							queryToAddGroup.exec();
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

	bot.getEvents().onCommand("groups", [&bot, &dataBase, &confirmationCode, &dataBasesAndColumnsNames](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				//string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

				//if (confirmationCode != "ERROR" && code == confirmationCode)
				//{
				//	try
				//	{
				//		SQLite::Statement query{ *dataBase, "INSERT INTO " + dataBasesAndColumnsNames[0].first + " (" + dataBasesAndColumnsNames[0].second[0] + ',' + dataBasesAndColumnsNames[0].second[1] + ',' + dataBasesAndColumnsNames[0].second[2] + ") VALUES(? , ? , ? )" };

				//		query.bind(1, message->from->id);
				//		query.bind(2, message->from->firstName);
				//		query.bind(3, message->from->lastName);
				//		query.exec();

				//		confirmationCode = "ERROR";

				//		Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " entered the correct confirmation code and became a moderator");

				//		//bot.getApi().sendMessage(message->chat->id, "Вы стали модератором.");
				//		SendManagerUI(bot, message->chat->id);
				//	}
				//	catch (exception& e)
				//	{
				//		Log({ LogPrefix::Database, LogPrefix::Error }, e.what());

				//		bot.getApi().sendMessage(message->chat->id, "Произошла техническая ошибка. Вы не стали модератором.");
				//	}
				//}
				//else if (confirmationCode != "ERROR" && !code.empty())
				//{
				//	Log({ LogPrefix::Bot, LogPrefix::Error }, "user: " + to_string(message->from->id) + " entered an incorrect confirmation code and did not become a moderator");
				//	bot.getApi().sendMessage(message->chat->id, "Код подтверждения неверен.");
				//}
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

	bot.getEvents().onMyChatMember([&bot, &dataBase, &dataBasesAndColumnsNames](TgBot::ChatMemberUpdated::Ptr upd)
		{
			if (upd->chat->type != Chat::Type::Private)
			{
				try
				{
					SQLite::Statement groupAvailabilityQuery{ *dataBase, "SELECT * FROM " + dataBasesAndColumnsNames[1].first + " WHERE " + dataBasesAndColumnsNames[1].second[0] + '=' + to_string(upd->chat->id) };

					if (groupAvailabilityQuery.executeStep())
					{
						cout << "my_chat_member: chat=" << upd->chat->id << " old=" << upd->oldChatMember->status << " new=" << upd->newChatMember->status << endl;

						dataBase->exec("UPDATE " + dataBasesAndColumnsNames[1].first + " SET " + dataBasesAndColumnsNames[1].second[1] + "='" + upd->chat->title + "'," + dataBasesAndColumnsNames[1].second[2] + "='" + to_string(bot.getApi().getChatMember(upd->chat->id, bot.getApi().getMe()->id)->status == "administrator") + "' WHERE " + dataBasesAndColumnsNames[1].second[0] + '=' + to_string(upd->chat->id));

						//SQLite::Statement queryToUpdateGroup{ *dataBase, "UPDATE " + dataBasesAndColumnsNames[1].first + " SET " + dataBasesAndColumnsNames[1].second[1] + "='" + upd->chat->title + "'," + dataBasesAndColumnsNames[1].second[2] + "='" + to_string(bot.getApi().getChatMember(upd->chat->id, bot.getApi().getMe()->id)->status == "administrator") + "' WHERE " + dataBasesAndColumnsNames[1].second[0] + '=' + to_string(upd->chat->id) };
						//queryToUpdateGroup.exec();
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
					cout << "delete" << '\n';
					bot.getApi().deleteMessage(chatId, message->messageId);

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


