# GroupModerBot

<p align="left">
  <a href="https://github.com/H-D-OWL/GroupModerBot/releases/latest"><img src="https://img.shields.io/github/v/release/H-D-OWL/GroupModerBot?style=flat-square&color=2ea44f"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/H-D-OWL/GroupModerBot?style=flat-square&color=blue&v=1"></a>
  <br>
  <img src="https://img.shields.io/badge/Platform-Linux-Fee600?style=flat-square&logo=linux&logoColor=white">
  <img src="https://img.shields.io/badge/Platform-Windows_x64-0078D6?style=flat-square&logo=windows&logoColor=white">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=c%2B%2B&logoColor=white">
  <img src="https://img.shields.io/badge/Database-SQLite-003B57?style=flat-square&logo=sqlite&logoColor=white">
  <img src="https://img.shields.io/badge/API-Telegram_Bot-2CA5E0?style=flat-square&logo=telegram&logoColor=white">
</p>

## 📋 Description

GroupModerBot is a reliable Telegram moderation bot designed to help you maintain order in your groups. It provides administrators with a flexible warning system, allowing them to issue warnings, and automatically mute or ban repeat offenders based on configurable thresholds.

## ⛓️ Requirements & Dependencies

### Operating Systems
* **Windows x64** (Tested on Windows 10)
* **Linux on GLIBC 2.35 or higher** (Tested on WSL2: Ubuntu 22.04 LTS and Ubuntu 24.04 LTS)

<details>
 <summary>Dependencies</summary>

 *The following dependencies are not required for the pre-built binaries to run.*

### Build Tools
* **Compiler** with **C++20** support (MSVC 19.30+, GCC 11+ and others)
* [CMake](https://github.com/kitware/cmake) v3.21+
* [Ninja](https://github.com/ninja-build/ninja) v1.10.1
* [vcpkg](https://github.com/microsoft/vcpkg/) 

### C++ Libraries
* [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) v3.3.3
* [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp) v1.10
* [Boost](https://www.boost.org/) (Required by tgbot-cpp and shipped with it)

### System Libraries
* [ZLIB](https://zlib.net/) v1.3.2
* [OpenSSL](https://www.openssl.org/) v3.6.2
* [CURL](https://curl.se/) v8.20.0

</details>

## ✨ Features
1. ⚖️ **Warning System** - Issue warnings to group members and set custom penalties. Includes an auto-mute feature where mute duration increases progressively using the Fibonacci sequence.
2. 🛡️ **Fault Tolerance** - Exception handling ensures it stays online continuously unless the host runs out of memory or power.
3. 🔒 **Persistent Storage** - All data (admins, warnings, group settings) is stored securely in an SQLite database. No data is lost upon restart or disconnection.

## 🚀 Quick Start
Follow these steps to launch the bot:

**1. Download the latest release**
* Go to the [Releases](../../releases).
* Download the `GroupModerBot_[Version]_[OperatingSystem].zip` archive and extract it to an empty folder. *The path to the folder with the binary file must not contain non-Latin letters. **Windows**: Do not separate the `.exe` from the `.dll` files.*			

**2. Get a bot token**
* Create a new bot in [@BotFather](https://t.me/BotFather) on Telegram.
* Send `/token` to get the bot token.

To function as a moderator, the bot needs specific permissions in BotFather:
* Send `/setjoingroups` -\> Select your bot -\> **Enable** (Allows adding the bot to groups).
* Send `/setprivacy` -\> Select your bot -\> **Disable** (Allows the bot to read all messages in the group, which is required for reply-based commands).
  
**3. Database setup**

* The bot requires an SQLite database to function. You have two options:

	* Automatic (Recommended): Simply skip this step. If the configuration file does not contain `DbPath=`, a file `GroupModerBotDatabase.db` containing the necessary structure will be automatically created in the folder with the binary file.
	* Manual: If you wish to use a custom location or filename, create a `.db` file (you can use [DB Browser](https://sqlitebrowser.org) or [SQLiteStudio](https://sqlitestudio.pl) for this, for example). *The path to `.db` must not contain non-Latin letters.* Then create the necessary tables in this database:

<details>
 <summary>BotAdmins</summary>
 
```sql
CREATE TABLE "BotAdmins" (
	"Id"	INTEGER NOT NULL UNIQUE,
	"FirstName"	TEXT NOT NULL,
	"LastName"	TEXT NOT NULL,
	"Username"	TEXT NOT NULL,
	"IsBot"	INTEGER NOT NULL,
	"IsPremium"	INTEGER NOT NULL,
	"IsBotOwner"	INTEGER NOT NULL,
	PRIMARY KEY("Id")
)
```
</details>

<details>
 <summary>Groups</summary>
 
```sql
CREATE TABLE "Groups" (
	"Id"	INTEGER NOT NULL UNIQUE,
	"Title"	TEXT NOT NULL,
	"UniqueTitle"	TEXT NOT NULL UNIQUE,
	"Type"	INTEGER NOT NULL CHECK("Type" >= 0 AND "Type" <= 3),
	"IsBotAdmin"	INTEGER NOT NULL CHECK("IsBotAdmin" = 0 OR "IsBotAdmin" = 1),
	"IsBotActive"	INTEGER NOT NULL CHECK("IsBotActive" = 0 OR "IsBotActive" = 1),
	PRIMARY KEY("Id")
)
```
</details>

<details>
 <summary>GroupsSettings</summary>
 
```sql
CREATE TABLE "GroupsSettings" (
	"Id"	INTEGER NOT NULL UNIQUE,
	"NumWarnToMute"	INTEGER NOT NULL CHECK("NumWarnToMute" >= 0),
	"NumWarnToBan"	INTEGER NOT NULL CHECK("NumWarnToBan" >= 0),
	FOREIGN KEY("Id") REFERENCES "Groups"("Id") ON DELETE CASCADE
)
```
</details>

<details>
 <summary>UserWarnings</summary>
 
```sql
CREATE TABLE "UsersWarnings" (
	"Id"	INTEGER NOT NULL,
	"GroupId"	INTEGER NOT NULL,
	"QuantityWarn"	INTEGER NOT NULL CHECK("QuantityWarn" >= 0),
	PRIMARY KEY("Id","GroupId"),
	FOREIGN KEY("GroupId") REFERENCES "Groups"("Id") ON DELETE CASCADE
)
```
</details>

**4. Configuration**
* Open the `DataForBot.txt` file in the folder with the binary file.
* Fill in your `BotToken`, (optionally) your `DbPath`, and (optionally) `EnableProcessPendingUpdates`:

```
DbPath=C:\Users\UserName\Desktop\Database\DBForBot.db
BotToken=1234567890:AAFJmnuH50H05MqFwJZrrpI2FTRGTFCWK68
EnableProcessPendingUpdates=true
```

* `EnableProcessPendingUpdates` - Enables or disables `ProcessPendingUpdates`. If `ProcessPendingUpdates` is enabled, the bot will ignore all commands and events sent to it while it was offline that are not directly related to the bot. 
The bot will only process its own movements within groups and changes to its status within groups. `true`, `t`, and `1` are defined as **true**. `false`, `f`, and `0` are defined as **false**.

* **IMPORTANT**:
	* Line order does not matter, but ensure there are no spaces around the `=` sign.
	* Any other text or empty lines in the file will be ignored.
	* If you remove `DbPath=` , a `.db` file will be automatically created as described in step 3 (Automatic).
	* If you remove `EnableProcessPendingUpdates=`, `ProcessPendingUpdates` will be considered enabled.

**5. Run & Authenticate**
* Run the `GroupModerBot` binary.
* If everything is configured correctly, you will see a successful initialization log in the console, ending with `[BOT] [EVENT] bot: "NameBot" has been launched`.
* Look for the **confirmation code** in the console output and copy it:
  `[PROGRAM] [EVENT] confirmation code: 255127673...`
* Open a private chat with your bot in Telegram and click **Start**.
* Claim ownership by sending the following command with your copied code:
  `/add_admin 255127673...`

The bot will reply: `you have become a bot owner`. You are now the owner of the bot and have full access to its functionality!

## ⌨️ Usage

### Bot Commands
**Syntax guide:** 
* Parameters: `< >` = required, `[ ]` = optional.
* Location: 🔒 = private chat, 👥 = groups.
* Access: 👑 = owner, 🛡️ = admin, 👤 = guest.

| Command & Parameters | Location | Access | Description |
| :--- | :--- | :--- | :--- |
| `/start` | 🔒 | 👑🛡️👤 | Show available commands |
| `/groups` | 🔒 | 👑🛡️ | List all groups containing the bot |
| `/set_group_unique_title <OldUniqueTitle> <NewUniqueTitle>` | 🔒 | 👑 | Change the `UniqueTitle` for a group |
| `/admins` | 🔒 | 👑🛡️ |  List all bot administrators |
| `/add_admin [AdminConfirmationCode]` | 🔒 | 👑👤 | 👑: Generate an AdminConfirmationCode. <br> 👤: Become the owner (if none exists) or an admin by entering the confirmation code |
| `/remove_admin <AdminNumber>` | 🔒 | 👑 | Remove an admin using their index number from `/admins`. <br> *Always use `/admins` first to get the correct number.* |
| `/set_warn_mute_settings <QuantityWarnToMute>` | 🔒 | 👑🛡️ |Set the number of warnings after which a group member will be muted. Default: 3. <br> Mute duration (days) = Fibonacci(UserWarns−QuantityWarnToMute). <br> **Example:** If QuantityWarnToMute is 3 and the user receives the 7th warning, mute is Fibonacci(4) = 3 days |
| `/set_warn_ban_settings <QuantityWarnToBan>` | 🔒 | 👑🛡️ | Set the number of warnings before banning a group member. Default: 5 |
| `/bot_active` | 👥 | 👑 | Activates the bot. The bot begins executing commands in the group |
| `/bot_deactive` | 👥 | 👑 | Deactivate the bot. The bot stops executing commands in the group |
| `/add_warn [QuantityWarns]` | 👥 | 👑🛡️ | Add the specified number of warnings to a member. Default: 1. <br> *Must be used as a reply to a user's message* |
| `/remove_warn [QuantityWarns]` | 👥 | 👑🛡️ | Remove the specified number of warnings from a member. Default: 1. <br> *Must be used as a reply to a user's message* |
| `/set_warn <QuantityWarns>` | 👥 | 👑🛡️ | Set the specified number of warnings for a member. <br> *Must be used as a reply to a user's message* |
| `/view_warn` | 👥 | 👑🛡️ | Check the current number of warnings for a member. <br> *Must be used as a reply to a user's message* |
| `/disable_bot` | 🔒 | 👑 | Turn off the bot completely |

**Parameters Reference:** 
* **UniqueTitle:** A unique string (1-32 characters). Allowed: `A-z`, `0-9`, and underscore `_`.
* **AdminConfirmationCode:** A unique 32-character numeric verification string.
* **AdminNumber:** The specific index number retrieved from the `/admins` list.
* **QuantityWarns:** An integer. Warnings: cannot be negative.

## 🕛 TODO
* Full multithreading
* Logging to a file

## 🤝 Contribution
* Found a bug or have a suggestion? Feel free to open an [issue](../../issues).

* If you want to support the project, please give it a star. ⭐
