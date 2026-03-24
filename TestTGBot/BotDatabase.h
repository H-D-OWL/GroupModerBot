#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <SQLiteCpp/SQLiteCpp.h>
#include <tgbot/tgbot.h>

#include "logging.h"

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

		Admin(int64_t id, string firstName, string lastName, string username, bool isBot, bool isPremium, bool isBotOwner) 
			: id(id),  firstName(firstName), lastName(lastName), username(username), isBot(isBot), isPremium(isPremium), isBotOwner(isBotOwner) {}
	};

	struct Group
	{
		int64_t id{};
		string title{}, uniqueTitle{};
		Chat::Type type{};
		bool isBotAdmin{}, isBotActive{};
		int64_t numWarnToMute{}, numWarnToBan{};

		Group(int64_t id, string title, string uniqueTitle, Chat::Type type, bool isBotAdmin, bool isBotActive, int64_t numWarnToMute, int64_t numWarnToBan)
			: id(id), title(title), uniqueTitle(uniqueTitle), type(type), isBotAdmin(isBotAdmin), isBotActive(isBotActive), numWarnToMute(numWarnToMute), numWarnToBan(numWarnToBan) {}
	};

	void Open(const string& pathToDatabase);
	void CheckStructure() const;
	void CacheLoad();

	bool TableHasColumn(const string& tableName, const string_view columnName) const;

	const Admin* GetAdmin(const int64_t userId) const;
	const unordered_map<int64_t, Admin>& GetAdmins() const;
	bool IsAdmin(const int64_t userId) const;
	bool IsOwner(const int64_t userId) const;
	size_t GetNumberAdmins() const;
	void AddAdmin(const Admin& user);
	void UpdateAdmin(const Admin& admin);
	void DeleteAdmin(const int64_t id);

	void AddGroup(const Group& group);
	void UpdateGroup(const Group& group);
	void DeleteGroup(const int64_t id);
	const unordered_map<int64_t, Group>& GetGroups() const;
	int64_t GroupIdFromUniqueTitle(const string& uniqueTitle) const;


private:

	// Adds admin data to the cache. 
	// The data must not already be in the cache and have an empty username.
	void AddAdminToCache(const Admin& admin);

	void UpdateAdminToCache(const Admin& admin);

	void DeleteAdminFromCache(const int64_t id);

	// Adds group data to the cache. 
	// The data must not already be in the cache.
	void AddGroupToCache(const Group& group);

	void UpdateGroupToCache(const Group& group);

	// Removes group data from the cache. 
	// The data must already be in the cache.
	void DeleteGroupFromCache(const int64_t id);

	struct Cache
	{
		inline static unordered_map<int64_t, Admin> admins{};
		//unordered_map<string, int64_t> adminIdsByUsername{};

		inline static unordered_map<int64_t, Group> groups{};
		inline static unordered_map<string, int64_t> groupIdsByUniqueTitle{};
	};

	struct Table
	{
		const string_view nameTable;
		const vector<string_view> columnNames;

		string GetColumnNamesBetweenCommas() const;
		string GetPlaceholders() const;
		string GetColumnsEqualPlaceholders() const;
	};

	struct GroupsTable : Table
	{
		static constexpr string_view idColumnName				= "Id";
		static constexpr string_view titleColumnName			= "Title";
		static constexpr string_view uniqueTitleColumnName		= "UniqueTitle";
		static constexpr string_view typeColumnName				= "Type";
		static constexpr string_view isBotAdminColumnName		= "IsBotAdmin";
		static constexpr string_view isBotActiveColumnName		= "IsBotActive";
		static constexpr string_view numWarnToMuteColumnName	= "NumWarnToMute";
		static constexpr string_view numWarnToBanColumnName		= "NumWarnToBan";

		GroupsTable() : Table{ "Groups", {idColumnName, titleColumnName, uniqueTitleColumnName, typeColumnName, isBotAdminColumnName, isBotActiveColumnName, numWarnToMuteColumnName, numWarnToBanColumnName} } {};
	};

	struct BotAdminsTable : Table
	{
		static constexpr string_view idColumnName			= "Id";
		static constexpr string_view firstNameColumnName	= "FirstName";
		static constexpr string_view lastNameColumnName		= "LastName";
		static constexpr string_view usernameColumnName		= "Username";
		static constexpr string_view isBotColumnName		= "IsBot";
		static constexpr string_view isPremiumColumnName	= "IsPremium";
		static constexpr string_view isBotOwnerColumnName	= "IsBotOwner";

		BotAdminsTable() : Table{ "BotAdmins", {idColumnName, firstNameColumnName, lastNameColumnName, usernameColumnName, isBotColumnName, isPremiumColumnName, isBotOwnerColumnName} } {};
	};

	inline static const BotAdminsTable BotAdmins{};
	inline static const GroupsTable Groups{};

	const vector<const Table*> tables{
		&BotAdmins,
		&Groups
	};

	unique_ptr<SQLite::Database> botDatabase{};

	inline static Cache Cache{};
};
