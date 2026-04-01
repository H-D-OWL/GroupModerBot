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

	struct DataTable 
	{
		auto operator<=>(const DataTable&) const = default;
	};

	struct Admin : DataTable
	{
		int64_t id{};
		string firstName{}, lastName{}, username{};
		bool isBot{}, isPremium{}, isBotOwner{};

		Admin() = default;

		Admin(int64_t id, string firstName, string lastName, string username, bool isBot, bool isPremium, bool isBotOwner) 
			: id(id),  firstName(firstName), lastName(lastName), username(username), isBot(isBot), isPremium(isPremium), isBotOwner(isBotOwner) {}
	};

	struct Group : DataTable
	{
		int64_t id{};
		string title{}, uniqueTitle{};
		Chat::Type type{};
		bool isBotAdmin{}, isBotActive{};

		auto operator<=>(const Group&) const = default;

		Group() = default;

		Group(int64_t id, string title, string uniqueTitle, Chat::Type type, bool isBotAdmin, bool isBotActive)
			: id(id), title(title), uniqueTitle(uniqueTitle), type(type), isBotAdmin(isBotAdmin), isBotActive(isBotActive) {}
	};

	struct GroupSettings : DataTable
	{
		int64_t id{}, numWarnToMute{}, numWarnToBan{};

		auto operator<=>(const GroupSettings&) const = default;

		GroupSettings() = default;

		GroupSettings(int64_t id, int64_t numWarnToMute, int64_t numWarnToBan)
			: id(id), numWarnToMute(numWarnToMute), numWarnToBan(numWarnToBan) {}
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

	void AddGroup(const Group& group, const GroupSettings& groupSettings);
	void UpdateGroup(const Group& group, const GroupSettings& groupSettings);
	void DeleteGroup(const int64_t id);
	const unordered_map<int64_t, Group>& GetGroups() const;
	const unordered_map<int64_t, GroupSettings>& GetGroupsSettings() const;
	int64_t GroupIdFromUniqueTitle(const string& uniqueTitle) const;

	void SetWarns(const int64_t userId, const int64_t groupId, const int64_t warns);
	void DeleteWarns(const int64_t userId, const int64_t groupId) const;
	int64_t GetWarns(const int64_t userId, const int64_t groupId) const;


private:

	void UpsertCache(const Admin& admin);
	void UpsertCache(const Group& group);
	void UpsertCache(const GroupSettings& groupSettings);

	void DeleteAdminFromCache(const int64_t id);

	// Removes group data from the cache. 
	// The data must already be in the cache.
	void DeleteGroupFromCache(const int64_t id);

	struct Cache
	{
		inline static unordered_map<int64_t, Admin> admins{};
		//unordered_map<string, int64_t> adminIdsByUsername{};

		inline static unordered_map<int64_t, Group> groups{};
		inline static unordered_map<string, int64_t> groupIdsByUniqueTitle{};

		inline static unordered_map<int64_t, GroupSettings> groupsSettings{};
	};
	
	struct TableName
	{
		const string_view nameTable;
		const vector<string_view> columnNames;

		string GetColumnNamesBetweenCommas() const;
		string GetPlaceholders() const;
		string GetColumnsEqualPlaceholders() const;
	};

	struct BotAdminsTableName : TableName
	{
		static constexpr string_view idColumnName				= "Id";
		static constexpr string_view firstNameColumnName		= "FirstName";
		static constexpr string_view lastNameColumnName			= "LastName";
		static constexpr string_view usernameColumnName			= "Username";
		static constexpr string_view isBotColumnName			= "IsBot";
		static constexpr string_view isPremiumColumnName		= "IsPremium";
		static constexpr string_view isBotOwnerColumnName		= "IsBotOwner";

		BotAdminsTableName() : TableName{ "BotAdmins", {idColumnName, firstNameColumnName, lastNameColumnName, usernameColumnName, isBotColumnName, isPremiumColumnName, isBotOwnerColumnName} } {};
	};

	struct GroupsTableName : TableName
	{
		static constexpr string_view idColumnName				= "Id";
		static constexpr string_view titleColumnName			= "Title";
		static constexpr string_view uniqueTitleColumnName		= "UniqueTitle";
		static constexpr string_view typeColumnName				= "Type";
		static constexpr string_view isBotAdminColumnName		= "IsBotAdmin";
		static constexpr string_view isBotActiveColumnName		= "IsBotActive";

		GroupsTableName() : TableName{ "Groups", {idColumnName, titleColumnName, uniqueTitleColumnName, typeColumnName, isBotAdminColumnName, isBotActiveColumnName} } {};
	};

	struct GroupsSettingsTableName : TableName
	{
		static constexpr string_view idColumnName				= "Id";
		static constexpr string_view numWarnToMuteColumnName	= "NumWarnToMute";
		static constexpr string_view numWarnToBanColumnName		= "NumWarnToBan";

		GroupsSettingsTableName() : TableName{ "GroupsSettings", {idColumnName, numWarnToMuteColumnName, numWarnToBanColumnName} } {};
	};

	struct UsersWarningsTableName : TableName
	{
		static constexpr string_view idColumnName = "Id";
		static constexpr string_view groupIdColumnName = "GroupId";
		static constexpr string_view quantityWarnColumnName = "QuantityWarn";

		UsersWarningsTableName() : TableName{ "UsersWarnings", {idColumnName, groupIdColumnName, quantityWarnColumnName} } {};
	};

	inline static const BotAdminsTableName  botAdminsTableName{};
	inline static const GroupsTableName  groupsTableName{};
	inline static const GroupsSettingsTableName  groupsSettingsTableName{};
	inline static const UsersWarningsTableName  usersWarningsTableName{};

	const vector<const TableName*> tables{
		&botAdminsTableName,
		&groupsTableName,
		&groupsSettingsTableName,
		&usersWarningsTableName
	};

	unique_ptr<SQLite::Database> botDatabase{};

	inline static Cache Cache{};
};
