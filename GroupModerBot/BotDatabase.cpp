#include "BotDatabase.h"

void BotDatabase::Open(const string& pathToDatabase)
{
	if(pathToDatabase.empty())
		throw SQLite::Exception("pathToDatabase is empty");

	if (botDatabase)
		throw SQLite::Exception("database is already open");

	botDatabase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);

	SQLite::Statement query{ *botDatabase, "PRAGMA foreign_keys = ON" };

	query.exec();
}

void BotDatabase::CheckStructure() const
{
	for (const auto& table : tables)
	{
		const string& nameTable = string(table->nameTable);

		if (botDatabase->tableExists(nameTable))
		{
			for (const string_view column : table->columnNames)
			{
				if (!TableHasColumn(nameTable, column))
				{
					throw SQLite::Exception("table " + nameTable + " has no column named " + string(column));
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
	{
		SQLite::Statement adminsQuery{ *botDatabase,
			"SELECT "
			+ botAdminsTableName.GetColumnNamesBetweenCommas()
			+ " FROM "
			+ string(botAdminsTableName.nameTable) };

		while (adminsQuery.executeStep())
		{
			UpsertCache(Admin{
			adminsQuery.getColumn(0).getInt64(),
			adminsQuery.getColumn(1).getString(),
			adminsQuery.getColumn(2).getString(),
			adminsQuery.getColumn(3).getString(),
			static_cast<bool>(adminsQuery.getColumn(4).getInt64()),
			static_cast<bool>(adminsQuery.getColumn(5).getInt64()),
			static_cast<bool>(adminsQuery.getColumn(6).getInt64())
				});
		}
	}

	{
		SQLite::Statement groupsQuery{ *botDatabase,
			"SELECT "
			+ groupsTableName.GetColumnNamesBetweenCommas()
			+ " FROM "
			+ string(groupsTableName.nameTable) };

		while (groupsQuery.executeStep())
		{
			UpsertCache(Group{
			groupsQuery.getColumn(0).getInt64(),
			groupsQuery.getColumn(1).getString(),
			groupsQuery.getColumn(2).getString(),
			static_cast<Chat::Type>(groupsQuery.getColumn(3).getInt64()),
			static_cast<bool>(groupsQuery.getColumn(4).getInt64()),
			static_cast<bool>(groupsQuery.getColumn(5).getInt64()),
				});
		}
	}

	{
		SQLite::Statement groupsSettingsQuery{ *botDatabase,
		"SELECT "
		+ groupsSettingsTableName.GetColumnNamesBetweenCommas()
		+ " FROM "
		+ string(groupsSettingsTableName.nameTable) };

		while (groupsSettingsQuery.executeStep())
		{
			UpsertCache(GroupSettings{
			groupsSettingsQuery.getColumn(0).getInt64(),
			groupsSettingsQuery.getColumn(1).getInt64(),
			groupsSettingsQuery.getColumn(2).getInt64(),
				});
		}
	}
}

bool BotDatabase::TableHasColumn(const string& tableName, const string_view columnName) const
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

void BotDatabase::AddAdmin(const Admin& user)
{
	if (user.username.empty())
		throw runtime_error{ "The user must have a Telegram username (with @)" };

	if (IsAdmin(user.id))
		throw runtime_error{ "User " + user.username + " is already an administrator" };

	SQLite::Statement query{ *botDatabase, 
		"INSERT INTO " 
		+ string(botAdminsTableName.nameTable)
		+ " (" 
		+ botAdminsTableName.GetColumnNamesBetweenCommas()
		+ ") VALUES(" 
		+ botAdminsTableName.GetPlaceholders()
		+ ')'};

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
	if (!Cache.admins.contains(admin.id))
		throw SQLite::Exception("admin \"" + admin.username + "\" does not exists");

	SQLite::Statement query{ *botDatabase,
		"UPDATE "
		+ string(botAdminsTableName.nameTable)
		+ " SET "
		+ botAdminsTableName.GetColumnsEqualPlaceholders()
		+ " WHERE "
		+ string(botAdminsTableName.idColumnName)
		+ " = "
		+ to_string(admin.id) };

	query.bind(1, admin.id);
	query.bind(2, admin.firstName);
	query.bind(3, admin.lastName);
	query.bind(4, admin.username);
	query.bind(5, static_cast<int64_t>(admin.isBot));
	query.bind(6, static_cast<int64_t>(admin.isPremium));
	query.bind(7, static_cast<int64_t>(admin.isBotOwner));

	query.exec();

	UpsertCache(admin);
}

void BotDatabase::DeleteAdmin(const int64_t id)
{
	if (!Cache.admins.contains(id))
		throw SQLite::Exception("admin does not exist");

	SQLite::Statement query{ *botDatabase,
	"DELETE FROM "
	+ string(botAdminsTableName.nameTable)
	+ " WHERE "
	+ string(botAdminsTableName.idColumnName)
	+ " = ?" };

	query.bind(1, id);
	query.exec();

	DeleteAdminFromCache(id);
}

void BotDatabase::AddGroup(const Group& group, const GroupSettings& groupSettings)
{
	if (Cache.groups.contains(group.id))
		throw SQLite::Exception("group \"" + group.title + "\" already exists");

	if (Cache.groupIdsByUniqueTitle.contains(group.uniqueTitle))
		throw SQLite::Exception("group with unique title \"" + group.uniqueTitle + "\" already exists");

	{
		SQLite::Statement query{ *botDatabase,
			"INSERT INTO "
			+ string(groupsTableName.nameTable)
			+ " (" + groupsTableName.GetColumnNamesBetweenCommas()
			+ ") VALUES("
			+ groupsTableName.GetPlaceholders()
			+ ')' };

		query.bind(1, group.id);
		query.bind(2, group.title);
		query.bind(3, group.uniqueTitle);
		query.bind(4, static_cast<int64_t>(group.type));
		query.bind(5, static_cast<int64_t>(group.isBotAdmin));
		query.bind(6, static_cast<int64_t>(group.isBotActive));

		query.exec();

		UpsertCache(group);
	}

	{
		SQLite::Statement query{ *botDatabase,
			"INSERT INTO "
			+ string(groupsSettingsTableName.nameTable)
			+ " (" + groupsSettingsTableName.GetColumnNamesBetweenCommas()
			+ ") VALUES("
			+ groupsSettingsTableName.GetPlaceholders()
			+ ')' };

		query.bind(1, groupSettings.id);
		query.bind(2, groupSettings.numWarnToMute);
		query.bind(3, groupSettings.numWarnToBan);

		query.exec();

		UpsertCache(groupSettings);
	}
}

void BotDatabase::UpdateGroup(const Group& group, const GroupSettings& groupSettings)
{
	if (!Cache.groups.contains(group.id))
		throw SQLite::Exception("group \"" + group.uniqueTitle + "\" does not exists");

	if (!Cache.groupIdsByUniqueTitle.contains(Cache.groups.at(group.id).uniqueTitle))
		throw SQLite::Exception("unique title \"" + group.uniqueTitle + "\" does not exists");

	if (const auto it = Cache.groupIdsByUniqueTitle.find(group.uniqueTitle); it != Cache.groupIdsByUniqueTitle.end() && it->second != group.id)
		throw SQLite::Exception("group with unique title \"" + group.uniqueTitle + "\" already exists");

	if(Cache.groups.at(group.id) != group)
	{
		SQLite::Statement query{ *botDatabase,
			"UPDATE "
			+ string(groupsTableName.nameTable)
			+ " SET "
			+ groupsTableName.GetColumnsEqualPlaceholders()
			+ " WHERE "
			+ string(groupsTableName.idColumnName)
			+ " = "
			+ to_string(group.id) };

		query.bind(1, group.id);
		query.bind(2, group.title);
		query.bind(3, group.uniqueTitle);
		query.bind(4, static_cast<int64_t>(group.type));
		query.bind(5, static_cast<int64_t>(group.isBotAdmin));
		query.bind(6, static_cast<int64_t>(group.isBotActive));

		query.exec();

		UpsertCache(group);
	}

	if(Cache.groupsSettings.at(groupSettings.id) != groupSettings)
	{
		SQLite::Statement query{ *botDatabase,
			"UPDATE "
			+ string(groupsSettingsTableName.nameTable)
			+ " SET "
			+ groupsSettingsTableName.GetColumnsEqualPlaceholders()
			+ " WHERE "
			+ string(groupsSettingsTableName.idColumnName)
			+ " = "
			+ to_string(groupSettings.id) };

		query.bind(1, groupSettings.id);
		query.bind(2, groupSettings.numWarnToMute);
		query.bind(3, groupSettings.numWarnToBan);

		query.exec();

		UpsertCache(groupSettings);
	}
}

const unordered_map<int64_t, BotDatabase::Group>& BotDatabase::GetGroups() const
{
	return Cache.groups;
}

const unordered_map<int64_t, BotDatabase::GroupSettings>& BotDatabase::GetGroupsSettings() const
{
	return Cache.groupsSettings;
}

int64_t BotDatabase::GroupIdFromUniqueTitle(const string& uniqueTitle) const
{
	const auto it = Cache.groupIdsByUniqueTitle.find(uniqueTitle);

	if (it != Cache.groupIdsByUniqueTitle.end())
	{
		return it->second;
	}
	else
	{
		return 0;
	}	
}

void BotDatabase::SetWarns(const int64_t userId, const int64_t groupId, const int64_t warns)
{
	SQLite::Statement query{ *botDatabase,
	"INSERT INTO "
	+ string(usersWarningsTableName.nameTable)
	+ " (" + usersWarningsTableName.GetColumnNamesBetweenCommas()
	+ ") VALUES("
	+ usersWarningsTableName.GetPlaceholders()
	+ ") ON CONFLICT("
	+ string(usersWarningsTableName.idColumnName)
	+ ','
	+ string(usersWarningsTableName.groupIdColumnName)
	+ ") DO UPDATE SET "
	+ string(usersWarningsTableName.quantityWarnColumnName)
	+ "=?" };

	query.bind(1, userId);
	query.bind(2, groupId);
	query.bind(3, max(0ll, warns));

	query.bind(4, max(0ll, warns));

	query.exec();
}

void BotDatabase::DeleteWarns(const int64_t userId, const int64_t groupId) const
{
	SQLite::Statement query{ *botDatabase,
	"DELETE FROM "
	+ string(usersWarningsTableName.nameTable)
	+ " WHERE "
	+ string(usersWarningsTableName.idColumnName)
	+ " = ? AND "
	+ string(usersWarningsTableName.groupIdColumnName)
	+ " = ?" };

	query.bind(1, userId);
	query.bind(2, groupId);

	query.exec();
}

int64_t BotDatabase::GetWarns(const int64_t userId, const int64_t groupId) const
{
	SQLite::Statement query{ *botDatabase,
	"SELECT "
	+ string(usersWarningsTableName.quantityWarnColumnName)
	+ " FROM "
	+ string(usersWarningsTableName.nameTable) 
	+ " WHERE "
	+ string(usersWarningsTableName.idColumnName)
	+ " = ? AND "
	+ string(usersWarningsTableName.groupIdColumnName)
	+ " = ?" };

	query.bind(1, userId);
	query.bind(2, groupId);

	if (query.executeStep())
	{
		return query.getColumn(0).getInt64();
	}

	return 0;
}

void BotDatabase::DeleteGroup(const int64_t id)
{
	if (!Cache.groups.contains(id))
		throw SQLite::Exception("group does not exist");

	SQLite::Statement query{ *botDatabase, 
		"DELETE FROM " 
		+ string(groupsTableName.nameTable)
		+ " WHERE " 
		+ string(groupsTableName.idColumnName)
		+ " = ?" };

	query.bind(1, id);
	query.exec();
	
	DeleteGroupFromCache(id);
}

const BotDatabase::Admin* BotDatabase::GetAdmin(const int64_t userId) const
{
	const auto it = Cache.admins.find(userId);

	if (it != Cache.admins.end())
	{
		return &it->second;
	}
	else
	{
		return nullptr;
	}
}

const unordered_map<int64_t, BotDatabase::Admin>& BotDatabase::GetAdmins() const
{
	return Cache.admins;
}

bool BotDatabase::IsAdmin(const int64_t userId) const
{
	return Cache.admins.contains(userId);
}

bool BotDatabase::IsOwner(const int64_t userId) const
{
	return Cache.admins.contains(userId) && Cache.admins.at(userId).isBotOwner;
}

size_t BotDatabase::GetNumberAdmins() const
{
	return Cache.admins.size();
}

void BotDatabase::UpsertCache(const Admin& admin)
{
	Cache.admins[admin.id] = admin;
}

void BotDatabase::UpsertCache(const Group& group)
{
	auto it = Cache.groups.find(group.id);

	if (it != Cache.groups.end())
	{
		if (it->second.uniqueTitle != group.uniqueTitle)
		{
			const bool eraceUniqueTitle = Cache.groupIdsByUniqueTitle.erase(it->second.uniqueTitle);

			Cache.groupIdsByUniqueTitle[group.uniqueTitle] = group.id;

			assert(eraceUniqueTitle && "Cache desync");
		}
	}
	else
	{
		Cache.groupIdsByUniqueTitle[group.uniqueTitle] = group.id;
	}

	Cache.groups[group.id] = group;
}

void BotDatabase::UpsertCache(const GroupSettings& groupSettings)
{
	Cache.groupsSettings[groupSettings.id] = groupSettings;
}

void BotDatabase::DeleteAdminFromCache(const int64_t id)
{
	const bool eraseAdmin = Cache.admins.erase(id);

	assert(eraseAdmin && "Cache desync");
}

void BotDatabase::DeleteGroupFromCache(const int64_t id)
{
	const bool eraseGroupIdsByUniqueTitle = Cache.groupIdsByUniqueTitle.erase(Cache.groups.at(id).uniqueTitle);
	const bool eraseGroup = Cache.groups.erase(id);
	const bool eraseGroupSettings = Cache.groupsSettings.erase(id);

	assert(eraseGroupIdsByUniqueTitle && eraseGroup && eraseGroupSettings && "Cache desync");
}

string BotDatabase::TableName::GetColumnNamesBetweenCommas() const
{
	string AllColumnNames{};

	AllColumnNames.reserve(columnNames.size() * 5);

	for (size_t i = 0; i < columnNames.size(); ++i)
	{
		if (!AllColumnNames.empty()) { AllColumnNames += ','; }
		AllColumnNames += columnNames[i];
	}

	return AllColumnNames;
}

string BotDatabase::TableName::GetPlaceholders() const
{
	string Placeholders{};

	Placeholders.reserve(columnNames.size() * 2 - 1);

	for (size_t i = 0; i < columnNames.size(); ++i)
	{
		Placeholders += (i == 0 ? "?" : ",?");
	}

	return Placeholders;
}

string BotDatabase::TableName::GetColumnsEqualPlaceholders() const
{
	string ColumnsEqualValues{};

	ColumnsEqualValues.reserve(columnNames.size() * 10 - 1);

	for (size_t i = 0; i < columnNames.size(); ++i)
	{
		if (!ColumnsEqualValues.empty()) { ColumnsEqualValues += ","; }
		ColumnsEqualValues += columnNames[i];
		ColumnsEqualValues += "=?";
	}

	return ColumnsEqualValues;
}