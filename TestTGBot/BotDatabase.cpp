#include "BotDatabase.h"




	BotDatabase::BotDatabase()
	{
	}

	BotDatabase::~BotDatabase()
	{
	}

	const unique_ptr<SQLite::Database>& BotDatabase::Get() const
	{
		return botDatabase;
	}

	bool BotDatabase::Open(const string& pathToDatabase)
	{
		if (botDatabase)
			return false;
		botDatabase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);
		return true;
	}

	bool BotDatabase::CheckStructure()
	{
		for (const auto& data : tableAndcolumnNames)
		{
			if (botDatabase->tableExists(data.first))
			{
				for (const auto& column : data.second)
				{
					if (!TableHasColumn(data.first, column))
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

		return true;
	}

	bool BotDatabase::CacheReload()
	{
		SQLite::Statement adminsQuery{ *botDatabase, "SELECT " + BotAdministrators.columnNames[0] + " FROM " + BotAdministrators.tableName};

		while (adminsQuery.executeStep())
			Cache.adminIds.emplace(adminsQuery.getColumn(0).getInt64());

		SQLite::Statement groupsQuery{ *botDatabase, "SELECT " + Groups.columnNames[0] + ',' + Groups.columnNames[1] + " FROM " + Groups.tableName};

		while (groupsQuery.executeStep())
		{
			Cache.groupNames.emplace(groupsQuery.getColumn(1).getString(), groupsQuery.getColumn(0).getInt64());
			Cache.groupIds.emplace(groupsQuery.getColumn(0).getInt64());
		}

		return true;
	}

	bool BotDatabase::isTableEmpty(const string& tableName)
	{
		SQLite::Statement query(*botDatabase, "SELECT 1 FROM " + tableName + " LIMIT 1");
		return !query.executeStep();
	}

	bool BotDatabase::TableHasColumn(const string& tableName, const string& columnName)
	{
		SQLite::Statement query(*botDatabase, "PRAGMA table_info(" + tableName + ")");

		while (query.executeStep())
			if (query.getColumn(1).getText() == columnName)
				return true;

		return false;
	}

	const unordered_set<int64_t>& BotDatabase::GetAdminIds() const
	{
		return Cache.adminIds;
	}

	bool BotDatabase::SetAdmin(const int64_t memberId, const string& memberFirstName)
	{
		if (Cache.adminIds.find(memberId) == Cache.adminIds.cend())
		{
			SQLite::Statement query{ *botDatabase, "INSERT INTO " + BotAdministrators.tableName + " (" + BotAdministrators.columnNames[0] + ',' + BotAdministrators.columnNames[1] + ',' + BotAdministrators.columnNames[2] + ") VALUES(? , ? , ? )" };

			query.bind(1, memberId);
			query.bind(2, memberFirstName);
			query.bind(3, Cache.adminIds.empty());
			query.exec();

			Cache.adminIds.emplace(memberId);

			return true;
		}

		return false;
	}

	const unordered_set<string> BotDatabase::GetGroupNames() const
	{
		unordered_set<string> names{};
		for (const auto& a : Cache.groupNames)
			names.emplace(a.first);
		return names;
	}

	bool BotDatabase::AddGroup(const int64_t groupId, const string& groupName, const bool botIsAdmin)
	{
		SQLite::Statement queryToAddGroup{ *botDatabase, "INSERT INTO " + Groups.tableName + " (" + Groups.columnNames[0] + ',' + Groups.columnNames[1] + ',' + Groups.columnNames[2] + ") VALUES(? , ? , ? )" };
		queryToAddGroup.bind(1, groupId);
		queryToAddGroup.bind(2, groupName);
		queryToAddGroup.bind(3, botIsAdmin);
		queryToAddGroup.exec();

		Cache.groupIds.emplace(groupId);
		Cache.groupNames.emplace(groupName, groupId);

		return true;
	}

	bool BotDatabase::UpdateGroup(const int64_t groupId, const string& groupName, const bool botIsAdmin)
	{
		botDatabase->exec("UPDATE " + Groups.tableName + " SET " + Groups.columnNames[1] + "='" + groupName + "'," + Groups.columnNames[2] + "='" + to_string(botIsAdmin) + "' WHERE " + Groups.columnNames[0] + '=' + to_string(groupId));

		return true;
	}

	bool BotDatabase::DeleteGroup(const string& groupName)
	{
		SQLite::Statement query{ *botDatabase, "DELETE FROM " + Groups.tableName + " WHERE " + Groups.columnNames[1] + " LIKE ?" };
		query.bind(1, groupName);
		query.exec();

		Cache.groupIds.erase((*Cache.groupNames.find(groupName)).second);
		Cache.groupNames.erase(groupName);

		return true;
	}


