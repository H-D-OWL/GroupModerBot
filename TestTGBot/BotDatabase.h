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

		const unique_ptr<SQLite::Database>& Get() const;
		bool Open(const string& pathToDatabase);
		bool CheckStructure();
		bool CacheReload();


		bool isTableEmpty(const string& tableName);
		bool TableHasColumn(const string& tableName, const string& columnName);
		
		const unordered_set<int64_t>& GetAdminIds() const;
		bool SetAdmin(const int64_t memberId, const string& memberFirstName);
		
		const unordered_set<string> GetGroupNames() const;
		bool AddGroup(const int64_t groupId, const string& groupName, const bool botIsAdmin);
		bool UpdateGroup(const int64_t groupId, const string& groupName, const bool botIsAdmin);
		bool DeleteGroup(const string& groupName);


	private:

		//struct Group
		//{
		//	id Id;
		//	string Name;
		//};

		struct Cache
		{
			unordered_set<int64_t> adminIds{};
			unordered_set<int64_t> groupIds{};
			//unordered_map<int64_t, string> groupNames{};
			
			//unordered_map<id, Group> data;


			unordered_map<string, int64_t> groupNames;
		
		};

		struct Table
		{
			const string tableName{};
			const vector<string> columnNames{};
		};

		unique_ptr<SQLite::Database> botDatabase;

		Table BotAdministrators{ "BotAdministrators", {"AdministratorId", "AdministratorName", "IsOwner"} };
		Table Groups{ "Groups", {"GroupId", "GroupName", "BotIsAdministrator"} };

		const vector<pair<string, const vector<string>>> tableAndcolumnNames{
			{BotAdministrators.tableName, BotAdministrators.columnNames},
			{Groups.tableName, Groups.columnNames},
		};

		Cache Cache;
	};
