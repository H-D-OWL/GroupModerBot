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
		throw SQLite::Exception("pathToDatabase is empty");

	if (botDatabase)
		throw SQLite::Exception("database is already open");

	botDatabase = make_unique<SQLite::Database>(pathToDatabase, SQLite::OPEN_READWRITE);
	
	return true;
}

bool BotDatabase::CheckStructure() const
{
	for (const auto& table : tables)
	{
		if (botDatabase->tableExists(string(table->nameTable)))
		{
			for (const string_view column : table->columnNames)
			{
				if (!TableHasColumn(table->nameTable, column))
				{
					throw SQLite::Exception("table " + string(table->nameTable) + " has no column named " + string(column));
				}
			}
		}
		else
		{
			throw SQLite::Exception("no such table: " + string(table->nameTable));
		}
	}

	return true;
}

bool BotDatabase::CacheLoad()
{
	SQLite::Statement adminsQuery{ *botDatabase, "SELECT " + BotAdmins.GetColumnNamesBetweenCommas() + " FROM " + string(BotAdmins.nameTable) };

	while (adminsQuery.executeStep())
	{
		int num{ 0 };

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

	SQLite::Statement groupsQuery{ *botDatabase, "SELECT " + Groups.GetColumnNamesBetweenCommas() + " FROM " + string(Groups.nameTable) };

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

	return true;
}

bool BotDatabase::isTableEmpty(const string& tableName) const
{
	SQLite::Statement query(*botDatabase, "SELECT 1 FROM " + tableName + " LIMIT 1");

	return !query.executeStep();
}

bool BotDatabase::TableHasColumn(const string_view tableName, const string_view columnName) const
{
	SQLite::Statement query(*botDatabase, "PRAGMA table_info(" + string(tableName) + ")");

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

	SQLite::Statement query{ *botDatabase, "INSERT INTO " + string(BotAdmins.nameTable) + " (" + BotAdmins.GetColumnNamesBetweenCommas() + ") VALUES(" + BotAdmins.GetPlaceholders() + ')'};

	int num{ 1 };

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

	SQLite::Statement queryToAddGroup{ *botDatabase, "INSERT INTO " + string(Groups.nameTable) + " (" + Groups.GetColumnNamesBetweenCommas() + ") VALUES(" + Groups.GetPlaceholders() + ')' };

	int num{ 1 };

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
	botDatabase->exec("UPDATE " + string(Groups.nameTable) + " SET " + Groups.GetColumnsEqualValues({ to_string(group.id), group.title, group.uniqueTitle, to_string(static_cast<int64_t>(group.type)), to_string(static_cast<int64_t>(group.isBotAdmin)), to_string(group.isBotActive)}) + " WHERE " + string(Groups.idColumnName) + " = " + to_string(group.id));

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

	SQLite::Statement query{ *botDatabase, "DELETE FROM " + string(Groups.nameTable) + " WHERE " + string(Groups.idColumnName) + " = ?" };

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
	return Cache.admins.try_emplace(admin.id, admin).second;
}

bool BotDatabase::AddGroupToCache(const Group& group)
{
	return Cache.groups.try_emplace(group.id, group).second;
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

string BotDatabase::Table::GetColumnsEqualValues(const vector<string>& values) const
{
	string ColumnsEqualValues{};

	ColumnsEqualValues.reserve(columnNames.size() * 10 - 2);

	for (size_t i = 0; i < values.size(); ++i)
	{
		if (!ColumnsEqualValues.empty()) { ColumnsEqualValues += "',"; }
		ColumnsEqualValues += columnNames[i];
		ColumnsEqualValues += "='";
		ColumnsEqualValues += values[i];
	}

	return ColumnsEqualValues;
}
