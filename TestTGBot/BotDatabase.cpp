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
	{
		size_t num{ 0 };

		AddAdminToCache(Admin{
		.id = adminsQuery.getColumn(num++).getInt64(),
		.firstName = adminsQuery.getColumn(num++).getString(),
		.lastName = adminsQuery.getColumn(num++).getString(),
		.username = adminsQuery.getColumn(num++).getString(),
		.isBot = static_cast<bool>(adminsQuery.getColumn(num++).getInt64()),
		.isPremium = static_cast<bool>(adminsQuery.getColumn(num++).getInt64()),
		.isBotOwner = static_cast<bool>(adminsQuery.getColumn(num++).getInt64())
			});
	}

	SQLite::Statement groupsQuery{ *botDatabase, "SELECT " + Groups.GetColumnNames() + " FROM " + Groups.tableName };

	while (groupsQuery.executeStep())
	{
		size_t num{ 0 };

		AddGroupToCache(Group{
		.id				= groupsQuery.getColumn(num++).getInt64(),
		.title			= groupsQuery.getColumn(num++).getString(),
		.uniqueTitle	= groupsQuery.getColumn(num++).getString(),
		.type			= static_cast<Chat::Type>(groupsQuery.getColumn(num++).getInt64()),
		.isBotAdmin		= static_cast<bool>(groupsQuery.getColumn(num++).getInt64()),
		.isBotActive	= static_cast<bool>(groupsQuery.getColumn(num++).getInt64())
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

	size_t num{ 1 };

	query.bind(num++, member.id);
	query.bind(num++, member.firstName);
	query.bind(num++, member.lastName);
	query.bind(num++, member.username);
	query.bind(num++, member.isBot);
	query.bind(num++, member.isPremium);
	query.bind(num++, !GetNumberAdmins());
	query.exec();

	AddAdminToCache(Admin{
	.id			= member.id,
	.firstName	= member.firstName,
	.lastName	= member.lastName,
	.username	= member.username,
	.isBot		= member.isBot,
	.isPremium	= member.isPremium,
	.isBotOwner = !GetNumberAdmins()
		});

	return true;
}

bool BotDatabase::AddGroup(const Group& group)
{
	if (Cache.groups.contains(group.id))
		throw SQLite::Exception("group " + group.title + " already exists");

	SQLite::Statement queryToAddGroup{ *botDatabase, "INSERT INTO " + Groups.tableName + " (" + Groups.GetColumnNames() + ") VALUES(" + Groups.GetPlaceholders() + ')' };

	size_t num{ 1 };

	queryToAddGroup.bind(num++, group.id);
	queryToAddGroup.bind(num++, group.title);
	queryToAddGroup.bind(num++, group.uniqueTitle);
	queryToAddGroup.bind(num++, static_cast<int64_t>(group.type));
	queryToAddGroup.bind(num++, static_cast<int64_t>(group.isBotAdmin));
	queryToAddGroup.bind(num++, static_cast<int64_t>(false));
	queryToAddGroup.exec();

	AddGroupToCache(Group{
	.id				= group.id,
	.title			= group.title,
	.uniqueTitle	= group.uniqueTitle,
	.type			= group.type,
	.isBotAdmin		= group.isBotAdmin,
	.isBotActive	= false 
		});

	return true;
}

bool BotDatabase::UpdateGroup(const Group& group)
{
	botDatabase->exec("UPDATE " + Groups.tableName + " SET " + Groups.GetColumnsEqualValues({ to_string(group.id), group.title, group.uniqueTitle, to_string(static_cast<int64_t>(group.type)), to_string(static_cast<int64_t>(group.isBotAdmin)), to_string(group.isBotActive)}) + " WHERE " + Groups.columnNames[0] + " LIKE " + to_string(group.id));

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
	cachedGroup.uniqueTitle = group.uniqueTitle;
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

	for (auto i = 0; i < columnNames.size(); ++i)
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
