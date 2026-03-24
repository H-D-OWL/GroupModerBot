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
		AddAdminToCache(Admin{
		adminsQuery.getColumn(0).getInt64(),
		adminsQuery.getColumn(1).getString(),
		adminsQuery.getColumn(2).getString(),
		adminsQuery.getColumn(3).getString(),
		static_cast<bool>(adminsQuery.getColumn(4).getInt64()),
		static_cast<bool>(adminsQuery.getColumn(5).getInt64()),
		static_cast<bool>(adminsQuery.getColumn(6).getInt64())
			});
	}

	SQLite::Statement groupsQuery{ *botDatabase, 
		"SELECT " 
		+ Groups.GetColumnNamesBetweenCommas() 
		+ " FROM " 
		+ string(Groups.nameTable) };

	while (groupsQuery.executeStep())
	{
		AddGroupToCache(Group{
		groupsQuery.getColumn(0).getInt64(),
		groupsQuery.getColumn(1).getString(),
		groupsQuery.getColumn(2).getString(),
		static_cast<Chat::Type>(groupsQuery.getColumn(3).getInt64()),
		static_cast<bool>(groupsQuery.getColumn(4).getInt64()),
		static_cast<bool>(groupsQuery.getColumn(5).getInt64()),
		groupsQuery.getColumn(6).getInt64(),
		groupsQuery.getColumn(7).getInt64() 
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

	query.bind(1, user.id);
	query.bind(2, user.firstName);
	query.bind(3, user.lastName);
	query.bind(4, user.username);
	query.bind(5, static_cast<int64_t>(user.isBot));
	query.bind(6, static_cast<int64_t>(user.isPremium));
	query.bind(7, static_cast<int64_t>(user.isBotOwner));

	query.exec();

	AddAdminToCache(user);
}

void BotDatabase::UpdateAdmin(const Admin& admin)
{
	if (!Cache.admins.contains(admin.id))
		throw SQLite::Exception("admin \"" + admin.username + "\" does not exists");

	SQLite::Statement query{ *botDatabase,
		"UPDATE "
		+ string(BotAdmins.nameTable)
		+ " SET "
		+ BotAdmins.GetColumnsEqualPlaceholders()
		+ " WHERE "
		+ string(BotAdmins.idColumnName)
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

	UpdateAdminToCache(admin);
}

void BotDatabase::DeleteAdmin(const int64_t id)
{
	if (!Cache.admins.contains(id))
		throw SQLite::Exception("admin does not exist");

	SQLite::Statement query{ *botDatabase,
	"DELETE FROM "
	+ string(BotAdmins.nameTable)
	+ " WHERE "
	+ string(BotAdmins.idColumnName)
	+ " = ?" };

	query.bind(1, id);
	query.exec();

	DeleteAdminFromCache(id);
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

	queryToAddGroup.bind(1, group.id);
	queryToAddGroup.bind(2, group.title);
	queryToAddGroup.bind(3, group.uniqueTitle);
	queryToAddGroup.bind(4, static_cast<int64_t>(group.type));
	queryToAddGroup.bind(5, static_cast<int64_t>(group.isBotAdmin));
	queryToAddGroup.bind(6, static_cast<int64_t>(group.isBotActive));
	queryToAddGroup.bind(7, group.numWarnToMute);
	queryToAddGroup.bind(8, group.numWarnToBan);

	queryToAddGroup.exec();

	AddGroupToCache(group);
}

void BotDatabase::UpdateGroup(const Group& group)
{
	if (!Cache.groups.contains(group.id))
		throw SQLite::Exception("group \"" + group.uniqueTitle + "\" does not exists");

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

	query.bind(1, group.id);
	query.bind(2, group.title);
	query.bind(3, group.uniqueTitle);
	query.bind(4, static_cast<int64_t>(group.type));
	query.bind(5, static_cast<int64_t>(group.isBotAdmin));
	query.bind(6, static_cast<int64_t>(group.isBotActive));
	query.bind(7, group.numWarnToMute);
	query.bind(8, group.numWarnToBan);

	query.exec();

	UpdateGroupToCache(group);
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

void BotDatabase::AddAdminToCache(const Admin& admin)
{
	const bool emplaceAdmin = Cache.admins.try_emplace(admin.id, admin).second;

	assert(emplaceAdmin && "Cache desync");
}

void BotDatabase::UpdateAdminToCache(const Admin& admin)
{
	Cache.admins.at(admin.id) = admin;
}

void BotDatabase::DeleteAdminFromCache(const int64_t id)
{
	const bool eraseAdmin = Cache.admins.erase(id);

	assert(eraseAdmin && "Cache desync");
}

void BotDatabase::AddGroupToCache(const Group& group)
{
	const bool emplaceUniqueTitle = Cache.groupIdsByUniqueTitle.try_emplace(group.uniqueTitle, group.id).second;
	const bool emplaceGroup = Cache.groups.try_emplace(group.id, group).second;

	assert(emplaceUniqueTitle && emplaceGroup && "Cache desync");
}

void BotDatabase::UpdateGroupToCache(const Group& group)
{
	const bool eraceUniqueTitle = Cache.groupIdsByUniqueTitle.erase(Cache.groups.at(group.id).uniqueTitle);
	const bool emplaceGroup = Cache.groupIdsByUniqueTitle.try_emplace(group.uniqueTitle, group.id).second;

	Cache.groups.at(group.id) = group;

	assert(eraceUniqueTitle && emplaceGroup && "Cache desync");
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