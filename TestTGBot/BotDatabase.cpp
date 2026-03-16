#include "BotDatabase.h"

BotDatabase::BotDatabase()
{
}

BotDatabase::~BotDatabase()
{
}

void BotDatabase::Open(const string& pathToDatabase)
{
	if(pathToDatabase.empty())
		throw SQLite::Exception("pathToDatabase is empty");

	if (botDatabase)
		throw SQLite::Exception("database is already open");

	botDatabase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);
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
	SQLite::Statement adminsQuery{ *botDatabase, 
		"SELECT " 
		+ BotAdmins.GetColumnNamesBetweenCommas() 
		+ " FROM " 
		+ string(BotAdmins.nameTable) };

	while (adminsQuery.executeStep())
	{
		int num{ 0 };

		AddAdminToCache(Admin{
		.id				= adminsQuery.getColumn(num++).getInt64(),
		.firstName		= adminsQuery.getColumn(num++).getString(),
		.lastName		= adminsQuery.getColumn(num++).getString(),
		.username		= adminsQuery.getColumn(num++).getString(),
		.isBot			= static_cast<bool>(adminsQuery.getColumn(num++).getInt64()),
		.isPremium		= static_cast<bool>(adminsQuery.getColumn(num++).getInt64()),
		.isBotOwner		= static_cast<bool>(adminsQuery.getColumn(num++).getInt64())
			});
	}

	SQLite::Statement groupsQuery{ *botDatabase, 
		"SELECT " 
		+ Groups.GetColumnNamesBetweenCommas() 
		+ " FROM " 
		+ string(Groups.nameTable) };

	while (groupsQuery.executeStep())
	{
		int num{ 0 };

		AddGroupToCache(Group{
		.id				= groupsQuery.getColumn(num++).getInt64(),
		.title			= groupsQuery.getColumn(num++).getString(),
		.uniqueTitle	= groupsQuery.getColumn(num++).getString(),
		.type			= static_cast<Chat::Type>(groupsQuery.getColumn(num++).getInt64()),
		.isBotAdmin		= static_cast<bool>(groupsQuery.getColumn(num++).getInt64()),
		.isBotActive	= static_cast<bool>(groupsQuery.getColumn(num++).getInt64())
			});
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
		+ string(BotAdmins.nameTable) 
		+ " (" 
		+ BotAdmins.GetColumnNamesBetweenCommas() 
		+ ") VALUES(" 
		+ BotAdmins.GetPlaceholders() 
		+ ')'};

	int num{ 1 };

	query.bind(num++, user.id);
	query.bind(num++, user.firstName);
	query.bind(num++, user.lastName);
	query.bind(num++, user.username);
	query.bind(num++, user.isBot);
	query.bind(num++, user.isPremium);
	query.bind(num++, user.isBotOwner);
	query.exec();

	AddAdminToCache(user);
}

void BotDatabase::AddGroup(const Group& group)
{
	if (Cache.groups.contains(group.id))
		throw SQLite::Exception("group \"" + group.title + "\" already exists");

	if (Cache.groupIdsByUniqueTitle.contains(group.uniqueTitle))
		throw SQLite::Exception("group with unique title \"" + group.uniqueTitle + "\" already exists");

	SQLite::Statement queryToAddGroup{ *botDatabase, 
		"INSERT INTO " 
		+ string(Groups.nameTable) 
		+ " (" + Groups.GetColumnNamesBetweenCommas() 
		+ ") VALUES(" 
		+ Groups.GetPlaceholders() 
		+ ')' };

	int num{ 1 };

	queryToAddGroup.bind(num++, group.id);
	queryToAddGroup.bind(num++, group.title);
	queryToAddGroup.bind(num++, group.uniqueTitle);
	queryToAddGroup.bind(num++, static_cast<int64_t>(group.type));
	queryToAddGroup.bind(num++, static_cast<int64_t>(group.isBotAdmin));
	queryToAddGroup.bind(num++, static_cast<int64_t>(group.isBotActive));
	queryToAddGroup.exec();

	AddGroupToCache(group);
}

void BotDatabase::UpdateGroup(const Group& group)
{
	if (!Cache.groups.contains(group.id))
		throw SQLite::Exception("group \"" + group.title + "\" does not exists");

	if (!Cache.groupIdsByUniqueTitle.contains(Cache.groups.at(group.id).uniqueTitle))
		throw SQLite::Exception("unique title \"" + group.uniqueTitle + "\" does not exists");

	if (const auto it = Cache.groupIdsByUniqueTitle.find(group.uniqueTitle); it != Cache.groupIdsByUniqueTitle.end() && it->second != group.id)
		throw SQLite::Exception("group with unique title \"" + group.uniqueTitle + "\" already exists");

	SQLite::Statement query{ *botDatabase, 
		"UPDATE " 
		+ string(Groups.nameTable) 
		+ " SET " 
		+ Groups.GetColumnsEqualPlaceholders() 
		+ " WHERE " 
		+ string(Groups.idColumnName) 
		+ " = " 
		+ to_string(group.id)};

	int num{ 1 };

	query.bind(num++, group.id);
	query.bind(num++, group.title);
	query.bind(num++, group.uniqueTitle);
	query.bind(num++, static_cast<int64_t>(group.type));
	query.bind(num++, static_cast<int64_t>(group.isBotAdmin));
	query.bind(num++, static_cast<int64_t>(group.isBotActive));
	query.exec();

	Cache.groupIdsByUniqueTitle.erase(Cache.groups.at(group.id).uniqueTitle);
	Cache.groupIdsByUniqueTitle.try_emplace(group.uniqueTitle, group.id);

	Cache.groups.at(group.id) = group;
}

const unordered_map<int64_t, BotDatabase::Group>& BotDatabase::GetGroups() const
{
	return Cache.groups;
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

void BotDatabase::DeleteGroup(const int64_t id)
{
	if (!Cache.groups.contains(id))
		throw SQLite::Exception("group does not exist");

	SQLite::Statement query{ *botDatabase, 
		"DELETE FROM " 
		+ string(Groups.nameTable) 
		+ " WHERE " 
		+ string(Groups.idColumnName) 
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

bool BotDatabase::IsAdmin(const int64_t userId) const
{
	return Cache.admins.contains(userId);
}

size_t BotDatabase::GetNumberAdmins() const
{
	return Cache.admins.size();
}

void BotDatabase::AddAdminToCache(const Admin& admin)
{
	const bool emplaceAdmin = Cache.admins.try_emplace(admin.id, admin).second;

	assert(emplaceAdmin && "Cache desync");
}

void BotDatabase::AddGroupToCache(const Group& group)
{
	const bool emplaceUniqueTitle = Cache.groupIdsByUniqueTitle.try_emplace(group.uniqueTitle, group.id).second;
	const bool emplaceGroup = Cache.groups.try_emplace(group.id, group).second;

	assert(emplaceUniqueTitle && emplaceGroup && "Cache desync");
}

void BotDatabase::DeleteGroupFromCache(const int64_t id)
{
	const bool eraseGroup = Cache.groups.erase(id);

	assert(eraseGroup && "Cache desync");
}

string BotDatabase::Table::GetColumnNamesBetweenCommas() const
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

string BotDatabase::Table::GetPlaceholders() const
{
	string Placeholders{};

	Placeholders.reserve(columnNames.size() * 2 - 1);

	for (size_t i = 0; i < columnNames.size(); ++i)
	{
		Placeholders += (i == 0 ? "?" : ",?");
	}

	return Placeholders;
}

string BotDatabase::Table::GetColumnsEqualPlaceholders() const
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