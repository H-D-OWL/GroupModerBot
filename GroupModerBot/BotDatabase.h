#pragma once

#include <cstdint> 
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <tgbot/types/Chat.h> 

#include <SQLiteCpp/Database.h> 
 

namespace gmb
{

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
			std::string firstName{}, lastName{}, username{};
			bool isBot{}, isPremium{}, isBotOwner{};

			Admin() = default;

			Admin(int64_t id, std::string firstName, std::string lastName, std::string username, bool isBot, bool isPremium, bool isBotOwner)
				: id(id), firstName(firstName), lastName(lastName), username(username), isBot(isBot), isPremium(isPremium), isBotOwner(isBotOwner) {
			}
		};

		struct Group : DataTable
		{
			int64_t id{};
			std::string title{}, uniqueTitle{};
			TgBot::Chat::Type type{};
			bool isBotAdmin{}, isBotActive{};

			auto operator<=>(const Group&) const = default;

			Group() = default;

			Group(int64_t id, std::string title, std::string uniqueTitle, TgBot::Chat::Type type, bool isBotAdmin, bool isBotActive)
				: id(id), title(title), uniqueTitle(uniqueTitle), type(type), isBotAdmin(isBotAdmin), isBotActive(isBotActive) {
			}
		};

		struct GroupSettings : DataTable
		{
			int64_t id{}, numWarnToMute{}, numWarnToBan{};

			auto operator<=>(const GroupSettings&) const = default;

			GroupSettings() = default;

			GroupSettings(int64_t id, int64_t numWarnToMute, int64_t numWarnToBan)
				: id(id), numWarnToMute(numWarnToMute), numWarnToBan(numWarnToBan) {
			}
		};

		void Open(const std::string& pathToDatabase);
		void CheckStructure() const;
		void CacheLoad();

		bool TableHasColumn(const std::string& tableName, const std::string_view columnName) const;

		const Admin* GetAdmin(const int64_t userId) const;
		const std::unordered_map<int64_t, Admin>& GetAdmins() const;
		bool IsAdmin(const int64_t userId) const;
		bool IsOwner(const int64_t userId) const;
		size_t GetNumberAdmins() const;
		void AddAdmin(const Admin& user);
		void UpdateAdmin(const Admin& admin);
		void DeleteAdmin(const int64_t id);

		void AddGroup(const Group& group, const GroupSettings& groupSettings);
		void UpdateGroup(const Group& group, const GroupSettings& groupSettings);
		void DeleteGroup(const int64_t id);
		const Group* GetGroup(const int64_t id) const;
		const std::unordered_map<int64_t, Group>& GetGroups() const;
		const GroupSettings* GetGroupSettings(const int64_t id) const;
		const std::unordered_map<int64_t, GroupSettings>& GetGroupsSettings() const;
		int64_t GroupIdFromUniqueTitle(const std::string& uniqueTitle) const;
		bool IsBotActive(const int64_t groupId) const;

		void SetWarns(const int64_t userId, const int64_t groupId, const int64_t warns);
		void DeleteWarns(const int64_t userId, const int64_t groupId) const;
		int64_t GetWarns(const int64_t userId, const int64_t groupId) const;

	private:

		// Adds or updates data in the cache.
		void UpsertCache(const Admin& admin);

		// Adds or updates data in the cache.
		void UpsertCache(const Group& group);

		// Adds or updates data in the cache.
		void UpsertCache(const GroupSettings& groupSettings);

		// Removes admin data from the cache. 
		// The data must already be in the cache.
		void DeleteAdminFromCache(const int64_t id);

		// Removes group data from the cache. 
		// The data must already be in the cache.
		void DeleteGroupFromCache(const int64_t id);

		struct Cache
		{
			inline static std::unordered_map<int64_t, Admin> admins{};
			//std::unordered_map<std::string, int64_t> adminIdsByUsername{};

			inline static std::unordered_map<int64_t, Group> groups{};
			inline static std::unordered_map<std::string, int64_t> groupIdsByUniqueTitle{};

			inline static std::unordered_map<int64_t, GroupSettings> groupsSettings{};
		};

		struct TableName
		{
			const std::string_view nameTable;
			const std::vector<std::string_view> columnNames;

			std::string GetColumnNamesBetweenCommas() const;
			std::string GetPlaceholders() const;
			std::string GetColumnsEqualPlaceholders() const;
		};

		struct BotAdminsTableName : TableName
		{
			static constexpr std::string_view idColumnName = "Id";
			static constexpr std::string_view firstNameColumnName = "FirstName";
			static constexpr std::string_view lastNameColumnName = "LastName";
			static constexpr std::string_view usernameColumnName = "Username";
			static constexpr std::string_view isBotColumnName = "IsBot";
			static constexpr std::string_view isPremiumColumnName = "IsPremium";
			static constexpr std::string_view isBotOwnerColumnName = "IsBotOwner";

			BotAdminsTableName() : TableName{ "BotAdmins", {idColumnName, firstNameColumnName, lastNameColumnName, usernameColumnName, isBotColumnName, isPremiumColumnName, isBotOwnerColumnName} } {};
		};

		struct GroupsTableName : TableName
		{
			static constexpr std::string_view idColumnName = "Id";
			static constexpr std::string_view titleColumnName = "Title";
			static constexpr std::string_view uniqueTitleColumnName = "UniqueTitle";
			static constexpr std::string_view typeColumnName = "Type";
			static constexpr std::string_view isBotAdminColumnName = "IsBotAdmin";
			static constexpr std::string_view isBotActiveColumnName = "IsBotActive";

			GroupsTableName() : TableName{ "Groups", {idColumnName, titleColumnName, uniqueTitleColumnName, typeColumnName, isBotAdminColumnName, isBotActiveColumnName} } {};
		};

		struct GroupsSettingsTableName : TableName
		{
			static constexpr std::string_view idColumnName = "Id";
			static constexpr std::string_view numWarnToMuteColumnName = "NumWarnToMute";
			static constexpr std::string_view numWarnToBanColumnName = "NumWarnToBan";

			GroupsSettingsTableName() : TableName{ "GroupsSettings", {idColumnName, numWarnToMuteColumnName, numWarnToBanColumnName} } {};
		};

		struct UsersWarningsTableName : TableName
		{
			static constexpr std::string_view idColumnName = "Id";
			static constexpr std::string_view groupIdColumnName = "GroupId";
			static constexpr std::string_view quantityWarnColumnName = "QuantityWarn";

			UsersWarningsTableName() : TableName{ "UsersWarnings", {idColumnName, groupIdColumnName, quantityWarnColumnName} } {};
		};

		inline static const BotAdminsTableName  botAdminsTableName{};
		inline static const GroupsTableName  groupsTableName{};
		inline static const GroupsSettingsTableName  groupsSettingsTableName{};
		inline static const UsersWarningsTableName  usersWarningsTableName{};

		const std::vector<const TableName*> tables{
			&botAdminsTableName,
			&groupsTableName,
			&groupsSettingsTableName,
			&usersWarningsTableName
		};

		std::unique_ptr<SQLite::Database> botDatabase{};

		inline static Cache Cache{};
	};
}