#include "BotDatabase.h"

#include <algorithm> 
#include <cassert>
#include <cstdint> 
#include <memory>
#include <stdexcept> 
#include <string>
#include <string_view>
#include <unordered_map>

#include <tgbot/types/Chat.h> 

#include <SQLiteCpp/Database.h> 
#include <SQLiteCpp/Exception.h> 
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h> 

#include "Constants.h"

namespace gmb
{
	std::string BotDatabase::InitStandardDB()
	{
		SQLite::Database db(gmb::consts::standardDBFile, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

		const std::unordered_map<std::string_view, const std::string> queries{
			{"BotAdmins", R"(CREATE TABLE "BotAdmins" ("Id"	INTEGER NOT NULL UNIQUE,"FirstName"	TEXT NOT NULL,"LastName"	TEXT NOT NULL,"Username"	TEXT NOT NULL,"IsBot"	INTEGER NOT NULL,"IsPremium"	INTEGER NOT NULL,"IsBotOwner"	INTEGER NOT NULL,PRIMARY KEY("Id")))"},
			{"Groups", R"(CREATE TABLE "Groups" ("Id"	INTEGER NOT NULL UNIQUE,"Title"	TEXT NOT NULL,"UniqueTitle"	TEXT NOT NULL UNIQUE,"Type"	INTEGER NOT NULL CHECK("Type" >= 0 AND "Type" <= 3),"IsBotAdmin"	INTEGER NOT NULL CHECK("IsBotAdmin" = 0 OR "IsBotAdmin" = 1),"IsBotActive"	INTEGER NOT NULL CHECK("IsBotActive" = 0 OR "IsBotActive" = 1),PRIMARY KEY("Id")))"},
			{"GroupsSettings", R"(CREATE TABLE "GroupsSettings" ("Id"	INTEGER NOT NULL UNIQUE,"NumWarnToMute"	INTEGER NOT NULL CHECK("NumWarnToMute" >= 0),"NumWarnToBan"	INTEGER NOT NULL CHECK("NumWarnToBan" >= 0),FOREIGN KEY("Id") REFERENCES "Groups"("Id") ON DELETE CASCADE))"},
			{"UsersWarnings", R"(CREATE TABLE "UsersWarnings" ("Id"	INTEGER NOT NULL,"GroupId"	INTEGER NOT NULL,"QuantityWarn"	INTEGER NOT NULL CHECK("QuantityWarn" >= 0),PRIMARY KEY("Id","GroupId"),FOREIGN KEY("GroupId") REFERENCES "Groups"("Id") ON DELETE CASCADE))"}
		};

		for (const auto& table : tables)
		{
			assert(tables.size() == queries.size() && queries.contains(table->nameTable) && "Table standardDB desync");

			if (!db.tableExists(std::string(table->nameTable)))
			{
				SQLite::Statement query{ db, queries.at(table->nameTable)};

				query.exec();
			}
		}

		return gmb::consts::standardDBFile;
	}

	///

	void BotDatabase::Open(const std::string& pathToDatabase)
	{
		if (pathToDatabase.empty())
			throw SQLite::Exception("pathToDatabase is empty");

		if (botDatabase)
			throw SQLite::Exception("database is already open");

		botDatabase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);

		SQLite::Statement queryFK{ *botDatabase, "PRAGMA foreign_keys = ON" };

		queryFK.exec();

		SQLite::Statement queryWAL{ *botDatabase, "PRAGMA journal_mode = WAL" };

		queryWAL.executeStep();

		SQLite::Statement querySync{ *botDatabase, "PRAGMA synchronous = NORMAL" };
		querySync.exec();
	}

	void BotDatabase::CheckStructure() const
	{
		for (const auto& table : tables)
		{
			const std::string nameTable = std::string(table->nameTable);

			if (botDatabase->tableExists(nameTable))
			{
				for (const std::string_view column : table->columnNames)
				{
					if (!TableHasColumn(nameTable, column))
					{
						throw SQLite::Exception("table " + nameTable + " has no column named " + std::string(column));
					}
				}
			}
			else
			{
				throw SQLite::Exception("no such table: " + nameTable);
			}
		}
	}

	void BotDatabase::CacheLoad()
	{
		SQLite::Statement getAdmins{ *botDatabase,
			"SELECT "
			+ botAdminsTableName.GetColumnNamesBetweenCommas()
			+ " FROM "
			+ std::string(botAdminsTableName.nameTable) };

		while (getAdmins.executeStep())
		{
			UpsertCache(Admin{
			getAdmins.getColumn(0).getInt64(),
			getAdmins.getColumn(1).getString(),
			getAdmins.getColumn(2).getString(),
			getAdmins.getColumn(3).getString(),
			static_cast<bool>(getAdmins.getColumn(4).getInt64()),
			static_cast<bool>(getAdmins.getColumn(5).getInt64()),
			static_cast<bool>(getAdmins.getColumn(6).getInt64())
				});
		}

		//

		SQLite::Statement getGroups{ *botDatabase,
			"SELECT "
			+ groupsTableName.GetColumnNamesBetweenCommas()
			+ " FROM "
			+ std::string(groupsTableName.nameTable) };

		while (getGroups.executeStep())
		{
			UpsertCache(Group{
			getGroups.getColumn(0).getInt64(),
			getGroups.getColumn(1).getString(),
			getGroups.getColumn(2).getString(),
			static_cast<TgBot::Chat::Type>(getGroups.getColumn(3).getInt64()),
			static_cast<bool>(getGroups.getColumn(4).getInt64()),
			static_cast<bool>(getGroups.getColumn(5).getInt64()),
				});
		}

		//

		SQLite::Statement getGroupsSettings{ *botDatabase,
		"SELECT "
		+ groupsSettingsTableName.GetColumnNamesBetweenCommas()
		+ " FROM "
		+ std::string(groupsSettingsTableName.nameTable) };

		while (getGroupsSettings.executeStep())
		{
			UpsertCache(GroupSettings{
			getGroupsSettings.getColumn(0).getInt64(),
			getGroupsSettings.getColumn(1).getInt64(),
			getGroupsSettings.getColumn(2).getInt64(),
				});
		}
	}

	///

	bool BotDatabase::TableHasColumn(const std::string& tableName, const std::string_view columnName) const
	{
		SQLite::Statement query(*botDatabase,
			"PRAGMA table_info("
			+ tableName
			+ ")");

		while (query.executeStep())
			if (query.getColumn(1).getText() == columnName)
				return true;

		return false;
	}

	///

	const BotDatabase::Admin* BotDatabase::GetAdmin(const int64_t userId) const
	{
		const auto it = cache.admins.find(userId);

		if (it != cache.admins.end())
		{
			return &it->second;
		}
		else
		{
			return nullptr;
		}
	}

	const std::unordered_map<int64_t, BotDatabase::Admin>& BotDatabase::GetAdmins() const
	{
		return cache.admins;
	}

	bool BotDatabase::IsAdmin(const int64_t userId) const
	{
		return cache.admins.contains(userId);
	}

	bool BotDatabase::IsOwner(const int64_t userId) const
	{
		return cache.admins.contains(userId) && cache.admins.at(userId).isBotOwner;
	}

	size_t BotDatabase::GetNumberAdmins() const
	{
		return cache.admins.size();
	}

	void BotDatabase::AddAdmin(const Admin& user)
	{
		if (user.username.empty())
			throw std::runtime_error{ "The user must have a Telegram username (with @)" };

		if (IsAdmin(user.id))
			throw std::runtime_error{ "TgBot::User " + user.username + " is already an administrator" };

		SQLite::Statement query{ *botDatabase,
			"INSERT INTO "
			+ std::string(botAdminsTableName.nameTable)
			+ " ("
			+ botAdminsTableName.GetColumnNamesBetweenCommas()
			+ ") VALUES("
			+ botAdminsTableName.GetPlaceholders()
			+ ") "
			+ botAdminsTableName.GetOnConflictUpdateSet() };

		query.bind(1, user.id);
		query.bind(2, user.firstName);
		query.bind(3, user.lastName);
		query.bind(4, user.username);
		query.bind(5, static_cast<int64_t>(user.isBot));
		query.bind(6, static_cast<int64_t>(user.isPremium));
		query.bind(7, static_cast<int64_t>(user.isBotOwner));

		query.exec();

		UpsertCache(user);
	}

	void BotDatabase::UpdateAdmin(const Admin& admin)
	{
		if (!cache.admins.contains(admin.id))
			throw SQLite::Exception("admin \"" + admin.username + "\" does not exists");

		SQLite::Statement query{ *botDatabase,
			"UPDATE "
			+ std::string(botAdminsTableName.nameTable)
			+ " SET "
			+ botAdminsTableName.GetColumnsEqualPlaceholders()
			+ " WHERE "
			+ std::string(botAdminsTableName.idColumnName)
			+ " =?" };

		query.bind(1, admin.id);
		query.bind(2, admin.firstName);
		query.bind(3, admin.lastName);
		query.bind(4, admin.username);
		query.bind(5, static_cast<int64_t>(admin.isBot));
		query.bind(6, static_cast<int64_t>(admin.isPremium));
		query.bind(7, static_cast<int64_t>(admin.isBotOwner));

		query.bind(8, admin.id);

		query.exec();

		UpsertCache(admin);
	}

	void BotDatabase::DeleteAdmin(const int64_t id)
	{
		if (!cache.admins.contains(id))
			throw SQLite::Exception("admin does not exist");

		SQLite::Statement query{ *botDatabase,
		"DELETE FROM "
		+ std::string(botAdminsTableName.nameTable)
		+ " WHERE "
		+ std::string(botAdminsTableName.idColumnName)
		+ " = ?" };

		query.bind(1, id);
		query.exec();

		DeleteAdminFromCache(id);
	}

	///

	const BotDatabase::Group* BotDatabase::GetGroup(const int64_t id) const
	{
		const auto it = cache.groups.find(id);

		if (it != cache.groups.end())
		{
			return &it->second;
		}
		else
		{
			return nullptr;
		}
	}

	const std::unordered_map<int64_t, BotDatabase::Group>& BotDatabase::GetGroups() const
	{
		return cache.groups;
	}

	void BotDatabase::AddGroup(const Group& group, const GroupSettings& groupSettings)
	{
		if (cache.groups.contains(group.id))
			throw SQLite::Exception("group \"" + group.title + "\" already exists");

		if (cache.groupIdsByUniqueTitle.contains(group.uniqueTitle))
			throw SQLite::Exception("group with unique title \"" + group.uniqueTitle + "\" already exists");

		SQLite::Transaction transaction(*botDatabase);

		SQLite::Statement insertGroup{ *botDatabase,
			"INSERT INTO "
			+ std::string(groupsTableName.nameTable)
			+ " (" + groupsTableName.GetColumnNamesBetweenCommas()
			+ ") VALUES("
			+ groupsTableName.GetPlaceholders()
			+ ") "
			+ groupsTableName.GetOnConflictUpdateSet()};

		insertGroup.bind(1, group.id);
		insertGroup.bind(2, group.title);
		insertGroup.bind(3, group.uniqueTitle);
		insertGroup.bind(4, static_cast<int64_t>(group.type));
		insertGroup.bind(5, static_cast<int64_t>(group.isBotAdmin));
		insertGroup.bind(6, static_cast<int64_t>(group.isBotActive));

		insertGroup.exec();

		//

		SQLite::Statement insertGroupSettings{ *botDatabase,
			"INSERT INTO "
			+ std::string(groupsSettingsTableName.nameTable)
			+ " (" + groupsSettingsTableName.GetColumnNamesBetweenCommas()
			+ ") VALUES("
			+ groupsSettingsTableName.GetPlaceholders()
			+ ')' };

		insertGroupSettings.bind(1, groupSettings.id);
		insertGroupSettings.bind(2, groupSettings.numWarnToMute);
		insertGroupSettings.bind(3, groupSettings.numWarnToBan);

		insertGroupSettings.exec();

		transaction.commit();

		UpsertCache(group);
		UpsertCache(groupSettings);
	}

	void BotDatabase::UpdateGroup(const Group& group, const GroupSettings& groupSettings)
	{
		if (!cache.groups.contains(group.id))
			throw SQLite::Exception("group \"" + group.uniqueTitle + "\" does not exists");

		if (!cache.groupIdsByUniqueTitle.contains(cache.groups.at(group.id).uniqueTitle))
			throw SQLite::Exception("unique title \"" + group.uniqueTitle + "\" does not exists");

		if (const auto it = cache.groupIdsByUniqueTitle.find(group.uniqueTitle); it != cache.groupIdsByUniqueTitle.end() && it->second != group.id)
			throw SQLite::Exception("group with unique title \"" + group.uniqueTitle + "\" already exists");

		const bool groupHasChanged = cache.groups.at(group.id) != group;
		const bool groupsSettingsHasChanged = cache.groupsSettings.at(groupSettings.id) != groupSettings;

		SQLite::Transaction transaction(*botDatabase);

		if (groupHasChanged)
		{
			SQLite::Statement updateGroup{ *botDatabase,
				"UPDATE "
				+ std::string(groupsTableName.nameTable)
				+ " SET "
				+ groupsTableName.GetColumnsEqualPlaceholders()
				+ " WHERE "
				+ std::string(groupsTableName.idColumnName)
				+ " =?"};

			updateGroup.bind(1, group.id);
			updateGroup.bind(2, group.title);
			updateGroup.bind(3, group.uniqueTitle);
			updateGroup.bind(4, static_cast<int64_t>(group.type));
			updateGroup.bind(5, static_cast<int64_t>(group.isBotAdmin));
			updateGroup.bind(6, static_cast<int64_t>(group.isBotActive));
			
			updateGroup.bind(7, group.id);

			updateGroup.exec();
		}

		if (groupsSettingsHasChanged)
		{
			SQLite::Statement updateGroupSettings{ *botDatabase,
				"UPDATE "
				+ std::string(groupsSettingsTableName.nameTable)
				+ " SET "
				+ groupsSettingsTableName.GetColumnsEqualPlaceholders()
				+ " WHERE "
				+ std::string(groupsSettingsTableName.idColumnName)
				+ " =?" };

			updateGroupSettings.bind(1, groupSettings.id);
			updateGroupSettings.bind(2, groupSettings.numWarnToMute);
			updateGroupSettings.bind(3, groupSettings.numWarnToBan);
			
			updateGroupSettings.bind(4, groupSettings.id);

			updateGroupSettings.exec();
		}

		transaction.commit();

		if (groupHasChanged) UpsertCache(group);
		if (groupsSettingsHasChanged) UpsertCache(groupSettings);
	}

	void BotDatabase::DeleteGroup(const int64_t id)
	{
		if (!cache.groups.contains(id))
			throw SQLite::Exception("group does not exist");

		SQLite::Statement query{ *botDatabase,
			"DELETE FROM "
			+ std::string(groupsTableName.nameTable)
			+ " WHERE "
			+ std::string(groupsTableName.idColumnName)
			+ " = ?" };

		query.bind(1, id);
		query.exec();

		DeleteGroupFromCache(id);
	}

	///

	const BotDatabase::GroupSettings* BotDatabase::GetGroupSettings(const int64_t id) const
	{
		const auto it = cache.groupsSettings.find(id);

		if (it != cache.groupsSettings.end())
		{
			return &it->second;
		}
		else
		{
			return nullptr;
		}
	}

	const std::unordered_map<int64_t, BotDatabase::GroupSettings>& BotDatabase::GetGroupsSettings() const
	{
		return cache.groupsSettings;
	}

	///

	int64_t BotDatabase::GroupIdFromUniqueTitle(const std::string& uniqueTitle) const
	{
		const auto it = cache.groupIdsByUniqueTitle.find(uniqueTitle);

		if (it != cache.groupIdsByUniqueTitle.end())
		{
			return it->second;
		}
		else
		{
			return 0;
		}
	}

	bool BotDatabase::IsBotActive(const int64_t groupId) const
	{
		const auto it = cache.groups.find(groupId);

		if (it != cache.groups.end())
		{
			return it->second.isBotActive;
		}
		else
		{
			return false;
		}
	}

	///

	int64_t BotDatabase::GetWarns(const int64_t userId, const int64_t groupId) const
	{
		SQLite::Statement query{ *botDatabase,
		"SELECT "
		+ std::string(usersWarningsTableName.quantityWarnColumnName)
		+ " FROM "
		+ std::string(usersWarningsTableName.nameTable)
		+ " WHERE "
		+ std::string(usersWarningsTableName.idColumnName)
		+ " = ? AND "
		+ std::string(usersWarningsTableName.groupIdColumnName)
		+ " = ?" };

		query.bind(1, userId);
		query.bind(2, groupId);

		if (query.executeStep())
		{
			return query.getColumn(0).getInt64();
		}

		return 0;
	}

	void BotDatabase::SetWarns(const int64_t userId, const int64_t groupId, const int64_t warns)
	{
		SQLite::Statement query{ *botDatabase,
		"INSERT INTO "
		+ std::string(usersWarningsTableName.nameTable)
		+ " (" + usersWarningsTableName.GetColumnNamesBetweenCommas()
		+ ") VALUES("
		+ usersWarningsTableName.GetPlaceholders()
		+ ") ON CONFLICT("
		+ std::string(usersWarningsTableName.idColumnName)
		+ ','
		+ std::string(usersWarningsTableName.groupIdColumnName)
		+ ") DO UPDATE SET "
		+ std::string(usersWarningsTableName.quantityWarnColumnName)
		+ "=?" };

		query.bind(1, userId);
		query.bind(2, groupId);
		query.bind(3, std::max(static_cast<int64_t>(0), warns));

		query.bind(4, std::max(static_cast<int64_t>(0), warns));

		query.exec();
	}

	void BotDatabase::DeleteWarns(const int64_t userId, const int64_t groupId) const
	{
		SQLite::Statement query{ *botDatabase,
		"DELETE FROM "
		+ std::string(usersWarningsTableName.nameTable)
		+ " WHERE "
		+ std::string(usersWarningsTableName.idColumnName)
		+ " = ? AND "
		+ std::string(usersWarningsTableName.groupIdColumnName)
		+ " = ?" };

		query.bind(1, userId);
		query.bind(2, groupId);

		query.exec();
	}

	///

	void BotDatabase::UpsertCache(const Admin& admin)
	{
		cache.admins[admin.id] = admin;
	}

	void BotDatabase::UpsertCache(const Group& group)
	{
		auto it = cache.groups.find(group.id);

		if (it != cache.groups.end())
		{
			if (it->second.uniqueTitle != group.uniqueTitle)
			{
				[[maybe_unused]] const bool eraseUniqueTitle = cache.groupIdsByUniqueTitle.erase(it->second.uniqueTitle);

				cache.groupIdsByUniqueTitle[group.uniqueTitle] = group.id;

				assert(eraseUniqueTitle && "Cache desync");
			}
		}
		else
		{
			cache.groupIdsByUniqueTitle[group.uniqueTitle] = group.id;
		}

		cache.groups[group.id] = group;
	}

	void BotDatabase::UpsertCache(const GroupSettings& groupSettings)
	{
		cache.groupsSettings[groupSettings.id] = groupSettings;
	}

	void BotDatabase::DeleteAdminFromCache(const int64_t id)
	{
		[[maybe_unused]] const bool eraseAdmin = cache.admins.erase(id);

		assert(eraseAdmin && "Cache desync");
	}

	void BotDatabase::DeleteGroupFromCache(const int64_t id)
	{
		[[maybe_unused]] const bool eraseGroupIdsByUniqueTitle = cache.groupIdsByUniqueTitle.erase(cache.groups.at(id).uniqueTitle);
		[[maybe_unused]] const bool eraseGroup = cache.groups.erase(id);
		[[maybe_unused]] const bool eraseGroupSettings = cache.groupsSettings.erase(id);

		assert(eraseGroupIdsByUniqueTitle && eraseGroup && eraseGroupSettings && "Cache desync");
	}

	///

	std::string BotDatabase::TableName::GetColumnNamesBetweenCommas() const
	{
		assert(!columnNames.empty() && "columnNames is empty");

		std::string sql{};

		sql.reserve(columnNames.size() * 5);

		sql += columnNames[0];

		for (size_t i = 1; i < columnNames.size(); ++i)
		{
			sql += ',';
			sql += columnNames[i];
		}

		return sql;
	}

	std::string BotDatabase::TableName::GetPlaceholders() const
	{
		assert(!columnNames.empty() && "Table must have at least 1 columns");

		std::string sql{};

		sql.reserve(columnNames.size() * 2 - 1);

		sql += "?";

		for (size_t i = 1; i < columnNames.size(); ++i)
		{
			sql += ",?";
		}

		return sql;
	}

	std::string BotDatabase::TableName::GetColumnsEqualPlaceholders() const
	{
		assert(!columnNames.empty() && "Table must have at least 1 columns");

		std::string sql{};

		sql.reserve(columnNames.size() * 10 - 1);

		sql += columnNames[0];
		sql += "=?";

		for (size_t i = 1; i < columnNames.size(); ++i)
		{
			sql += ",";
			sql += columnNames[i];
			sql += "=?";
		}

		return sql;
	}

	std::string BotDatabase::TableName::GetOnConflictUpdateSet() const
	{
		assert(columnNames.size() > 1 && "Table must have at least 2 columns");

		std::string sql{"ON CONFLICT("};

		sql.reserve(columnNames.size() * 18 + 16);

		sql += columnNames[0];
		sql += ") DO UPDATE SET ";
		sql += columnNames[1];
		sql += "=excluded.";
		sql += columnNames[1];

		for (size_t i = 2; i < columnNames.size(); ++i)
		{
			sql += ",";
			sql += columnNames[i];
			sql += "=excluded.";
			sql += columnNames[i];
		}

		return sql;
	}
}