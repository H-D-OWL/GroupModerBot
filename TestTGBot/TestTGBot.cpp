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

/*
	1. Получение токина и пути к базе данных из файла DataForBot.txt.
	2. Проверка пути к базе данных и наличия в ней всех необходимых таблиц и столбчов, проверку токена.
	3. Выдача кода подтверждения для подтверждения прав первого администратора бота через телеграм в личных сообщениях.
	4.


*/






using namespace std;
using namespace TgBot;
using namespace logging;

bool DataBaseHasColumn(const SQLite::Database& dataBase, const string& tableName, const string& columnName)
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

	try
	{
		dataBase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);

		Log({ LogPrefix::Database, LogPrefix::Event }, "batabase: " + pathToDatabase.substr(pathToDatabase.rfind('\\') + 1) + " found");

		const unordered_map<string, vector<string>> dataBasesAndColumnsNames{
		{"Managers", {"IdManager", "FirstNameManager", "LastNameManager"}},
		};

		for (auto data : dataBasesAndColumnsNames)
		{
			if (dataBase->tableExists(data.first))
			{
				for (string column : data.second)
				{
					if (!DataBaseHasColumn(*dataBase, data.first, column))
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

		if (isTableEmpty(*dataBase, "Managers"))
		{
			random_device rd;
			default_random_engine dre(rd());
			uniform_int_distribution<int> uniform_dist('0', '9');

			confirmationCode.clear();

			for (int i = 0; i < 64; ++i)
			{
				confirmationCode += uniform_dist(dre);
			}

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
	

	//try 
	//{
	//	auto chat = bot.getApi().getChat(6690609226);
	//	Log({ LogPrefix::Bot, LogPrefix::Event }, chat->username + ' ' + chat->firstName + ' ' + chat->lastName);

	//}
	//catch (const TgBot::TgException& e) {
	//	// Пользователь не найден или бот не имеет доступа

	//}

	// ============================================================
	//Управление ботом
	// ============================================================

	bot.getEvents().onCommand("start", [&bot, &dataBase, &confirmationCode](Message::Ptr message)
		{
			if (message->chat->type == Chat::Type::Private)
			{
				string code = message->text.substr(message->text.size() == 6 ? 6 : 7);

				if (confirmationCode != "ERROR" && code == confirmationCode)
				{
					try
					{
						SQLite::Statement query{ *dataBase, "INSERT INTO Managers (IdManager, FirstNameManager, LastNameManager) VALUES (?, ?, ?)" };

						query.bind(1, message->from->id);
						query.bind(2, message->from->firstName);
						query.bind(3, message->from->lastName);
						query.exec();

						confirmationCode = "ERROR";

						Log({ LogPrefix::Bot, LogPrefix::Event }, "user: " + to_string(message->from->id) + ' ' + message->from->firstName + ' ' + message->from->lastName + " entered the correct confirmation code and became a moderator");

						bot.getApi().sendMessage(message->chat->id, "Вы стали модератором.");
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

	bot.getEvents().onMyChatMember([&bot](TgBot::ChatMemberUpdated::Ptr upd) {

		cout << "Action 1" << "\n";

		//cout << "my_chat_member: chat=" << upd->chat->id << " old=" << upd->oldChatMember->status << " new=" << upd->newChatMember->status << endl;

		//if(upd->newChatMember->status == "creator" || upd->newChatMember->status == "administrator")
		//{
		//	auto member = bot.getApi().getChatMember(upd->chat->id, bot.getApi().getMe()->id);
		//	auto adminPtr = dynamic_pointer_cast<ChatMemberAdministrator>(member);

		//	if (adminPtr)
		//	{
		//		//adminPtr->
		//		// теперь доступ к полям администратора
		//		bool canDelete = adminPtr->canDeleteMessages;
		//		cout << canDelete << "\n";
		//		// ...
		//	}
		//	return;

		//}
		//else if (upd->newChatMember->status == "member" || upd->newChatMember->status == "restricted")
		//{
		//	cout << upd->newChatMember->status << "\n";
		//	return;
		//}
		//else if (upd->newChatMember->status == "left" || upd->newChatMember->status == "kicked")
		//{
		//	cout << upd->newChatMember->status << "\n";

		//	return;
		//}

		//
		//throw "Unknown chat status";
		});

	bot.getEvents().onNonCommandMessage([&bot](Message::Ptr message)
		{
			try
			{

				const auto dataBot = bot.getApi().getMe();
				const string messageText = message->text;
				const Chat::Type chatType = message->chat->type;
				const int64_t chatId = message->chat->id;

				//cout << message->photo.empty() << "\n";

				//if (!message->photo.empty())
				//{
				//	bot.getApi().sendPhoto(chatId, message->photo[0]->fileId);
				//}
				//bot.getApi().sendMessage(chatId, "Your message contained: \"" + messageText + "\"");

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

		TgLongPoll longPoll(bot);

		while (true)
		{
			longPoll.start();
		}
	}
	catch (TgException& e)
	{
		Log({ LogPrefix::Bot, LogPrefix::Error }, e.what());
	}

	return 0;
}


