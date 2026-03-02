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
	if(pathToDatabase.empty())
		throw SQLite::Exception("database is already open");

	if (botDatabase)
		throw SQLite::Exception("database is already open");

	botDatabase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);
	
	return true;
}

bool BotDatabase::CheckStructure() const
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

bool BotDatabase::CacheLoad()
{
	SQLite::Statement adminsQuery{ *botDatabase, "SELECT " + BotAdministrators.GetColumnNames() + " FROM " + BotAdministrators.tableName };

	while (adminsQuery.executeStep())
		AddAdminToCache(Admin{
		.id				= adminsQuery.getColumn(0).getInt64(),
		.firstName		= adminsQuery.getColumn(1).getString(),
		.lastName		= adminsQuery.getColumn(2).getString(),
		.username		= adminsQuery.getColumn(3).getString(),
		.isBot			= static_cast<bool>(adminsQuery.getColumn(4).getInt64()),
		.isPremium		= static_cast<bool>(adminsQuery.getColumn(5).getInt64()),
		.isBotOwner		= static_cast<bool>(adminsQuery.getColumn(6).getInt64())
			});

	SQLite::Statement groupsQuery{ *botDatabase, "SELECT " + Groups.GetColumnNames() + " FROM " + Groups.tableName };

	while (groupsQuery.executeStep())
	{
		AddGroupToCache(Group{
		.id				= groupsQuery.getColumn(0).getInt64(),
		.title			= groupsQuery.getColumn(1).getString(),
		.type			= static_cast<Chat::Type>(groupsQuery.getColumn(2).getInt64()),
		.isBotAdmin		= static_cast<bool>(groupsQuery.getColumn(3).getInt64()),
		.isBotActive	= static_cast<bool>(groupsQuery.getColumn(4).getInt64())
			});
	}

	return true;
}

bool BotDatabase::isTableEmpty(const string& tableName) const
{
	SQLite::Statement query(*botDatabase, "SELECT 1 FROM " + tableName + " LIMIT 1");

	return !query.executeStep();
}

bool BotDatabase::TableHasColumn(const string& tableName, const string& columnName) const
{
	SQLite::Statement query(*botDatabase, "PRAGMA table_info(" + tableName + ")");

	while (query.executeStep())
		if (query.getColumn(1).getText() == columnName)
			return true;

	return false;
}

bool BotDatabase::AddAdmin(const Admin& member)
{
	if (member.username.empty())
		throw runtime_error{ "The user must have a Telegram username (with @)" };

	if (IsAdmin(member.id))
		throw runtime_error{ "User " + member.username + " is already an administrator" };

	SQLite::Statement query{ *botDatabase, "INSERT INTO " + BotAdministrators.tableName + " (" + BotAdministrators.GetColumnNames() + ") VALUES(" + BotAdministrators.GetPlaceholders() + ')'};

	query.bind(1, member.id);
	query.bind(2, member.firstName);
	query.bind(3, member.lastName);
	query.bind(4, member.username);
	query.bind(5, member.isBot);
	query.bind(6, member.isPremium);
	query.bind(7, !GetNumberAdmins());
	query.exec();

	AddAdminToCache(Admin{
	.id = member.id,
	.firstName = member.firstName,
	.lastName = member.lastName,
	.username = member.username,
	.isBot = member.isBot,
	.isPremium = member.isPremium,
	.isBotOwner = !GetNumberAdmins()
		});

	return true;
}

bool BotDatabase::AddGroup(const Group& group)
{
	if (Cache.groups.contains(group.id))
		throw SQLite::Exception("group " + group.title + " already exists");

	SQLite::Statement queryToAddGroup{ *botDatabase, "INSERT INTO " + Groups.tableName + " (" + Groups.GetColumnNames() + ") VALUES(" + Groups.GetPlaceholders() + ')' };

	queryToAddGroup.bind(1, group.id);
	queryToAddGroup.bind(2, group.title);
	queryToAddGroup.bind(3, static_cast<int64_t>(group.type));
	queryToAddGroup.bind(4, static_cast<int64_t>(group.isBotAdmin));
	queryToAddGroup.bind(5, static_cast<int64_t>(false));
	queryToAddGroup.exec();

	AddGroupToCache(Group{
	.id = group.id,
	.title = group.title,
	.type = group.type,
	.isBotAdmin = group.isBotAdmin,
	.isBotActive = false 
		});

	return true;
}

bool BotDatabase::UpdateGroup(const Group& group)
{
	botDatabase->exec("UPDATE " + Groups.tableName + " SET " + Groups.GetColumnsEqualValues({ to_string(group.id), group.title, to_string(static_cast<int64_t>(group.type)), to_string(static_cast<int64_t>(group.isBotAdmin)), to_string(group.isBotActive)}) + " WHERE " + Groups.columnNames[0] + " LIKE " + to_string(group.id));

	UpdateGroupFromCache(group);

	return true;
}

unordered_map<int64_t, BotDatabase::Group> BotDatabase::GetGroups() const
{
	return Cache.groups;
}

bool BotDatabase::DeleteGroup(const int64_t id)
{
	if (!Cache.groups.contains(id))
		throw SQLite::Exception("group does not exist");

	SQLite::Statement query{ *botDatabase, "DELETE FROM " + Groups.tableName + " WHERE " + Groups.columnNames[0] + " LIKE ?" };

	query.bind(1, id);
	query.exec();

	DeleteGroupFromCache(id);

	return true;
}

BotDatabase::Admin BotDatabase::GetAdmin(const int64_t memberId) const
{
	return (*Cache.admins.find(memberId)).second;
}

bool BotDatabase::IsAdmin(const int64_t memberId) const
{
	return Cache.admins.contains(memberId);
}

size_t BotDatabase::GetNumberAdmins() const
{
	return Cache.admins.size();
}

bool BotDatabase::AddAdminToCache(const Admin& admin)
{
	return Cache.admins.emplace(admin.id, admin).second;
}

bool BotDatabase::AddGroupToCache(const Group& group)
{
	return Cache.groups.emplace(group.id, group).second;
}

bool BotDatabase::UpdateGroupFromCache(const Group& group)
{
	auto& cachedGroup = (*Cache.groups.find(group.id)).second;

	cachedGroup.title = group.title;
	cachedGroup.type = group.type;
	cachedGroup.isBotAdmin = group.isBotAdmin;
	cachedGroup.isBotActive = group.isBotActive;

	return true;
}

bool BotDatabase::DeleteGroupFromCache(const int64_t id)
{
	return Cache.groups.erase(id);
}

string BotDatabase::Table::GetColumnNames() const
{
	string AllColumnNames{};

	for (const auto& column : columnNames)
	{
		AllColumnNames += column + ',';
	}

	AllColumnNames.pop_back();

	return AllColumnNames;
}

string BotDatabase::Table::GetPlaceholders() const
{
	string Placeholders{};

	for (auto a = 0; a < columnNames.size(); ++a)
	{
		Placeholders += "?,";
	}

	Placeholders.pop_back();

	return Placeholders;
}

string BotDatabase::Table::GetColumnsEqualValues(const vector<string> values) const
{
	string ColumnsEqualValues{};

	for (auto i = 0; i < columnNames.size(); ++i)
	{
		ColumnsEqualValues += columnNames[i] + "='" + values[i] + "',";
	}

	ColumnsEqualValues.pop_back();

	return ColumnsEqualValues;
}
