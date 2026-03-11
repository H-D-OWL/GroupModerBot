#pragma once

#include <iostream>
#include <vector>
#include <string_view>
#include <tgbot/tgbot.h>
#include "logging.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <unordered_set>
#include <unordered_map>

using namespace std;
using namespace TgBot;
using namespace logging;

	class BotDatabase
	{
	public:
		BotDatabase();
		~BotDatabase();

		struct Admin
		{
			int64_t id{};
			string firstName{}, lastName{}, username{};
			bool isBot{}, isPremium{}, isBotOwner{};
		};

		struct Group
		{
			int64_t id{};
			string title{}, uniqueTitle{};
			Chat::Type type{};
			bool isBotAdmin{}, isBotActive{};
		};

		const unique_ptr<SQLite::Database>& Get() const;
		bool Open(const string& pathToDatabase);
		bool CheckStructure() const;
		bool CacheLoad();


		bool isTableEmpty(const string& tableName) const;
		bool TableHasColumn(const string& tableName, const string& columnName) const;
		
		Admin GetAdmin(const int64_t memberId) const;
		bool IsAdmin(const int64_t memberId) const;
		size_t GetNumberAdmins() const;
		bool AddAdmin(const Admin& member);

		bool AddGroup(const Group& group);
		bool UpdateGroup(const Group& group);
		unordered_map<int64_t, Group> GetGroups() const;
		bool DeleteGroup(const int64_t id);

	private:

		bool AddAdminToCache(const Admin& admin);

		bool AddGroupToCache(const Group& group);
		bool UpdateGroupFromCache(const Group& group);
		bool DeleteGroupFromCache(const int64_t id);


		struct Cache
		{
			unordered_map<int64_t, Admin> admins{};
			//unordered_map<string, int64_t> adminIdsByUsername{};

			unordered_map<int64_t, Group> groups{};
			//unordered_map<string, int64_t> groupIdsByTitle{};		
		};

		struct Table
		{
			const string tableName{};
			const vector<string> columnNames{};

			string GetColumnNames() const;
			string GetPlaceholders() const;
			string GetColumnsEqualValues(const vector<string> values) const;
		};

		unique_ptr<SQLite::Database> botDatabase;

		Table BotAdministrators{ "BotAdministrators", {"Id", "FirstName", "LastName", "Username", "IsBot", "IsPremium", "IsBotOwner"} };
		Table Groups{ "Groups", {"Id", "Title", "UniqueTitle", "Type", "IsBotAdmin", "IsBotActive"}};

		const vector<pair<string, const vector<string>>> tableAndcolumnNames{
			{BotAdministrators.tableName, BotAdministrators.columnNames},
			{Groups.tableName, Groups.columnNames},
		};

		Cache Cache;
	};
