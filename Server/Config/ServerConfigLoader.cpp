#include "ServerConfigLoader.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
	using WarningList = std::vector<server::config::ServerConfigWarning>;

	void AddWarning(WarningList& warningList, std::size_t lineNumber, std::string message)
	{
		server::config::ServerConfigWarning warning{};
		warning.lineNumber = lineNumber;
		warning.message = std::move(message);

		warningList.push_back(std::move(warning));
	}

	[[nodiscard]] std::string MakeInvalidValueMessage(std::string_view section, std::string_view key, std::string_view value)
	{
		std::ostringstream stream;
		stream << "Invalid config value ignored. Section=[" << section << "], Key=" << key << ", Value=" << value;
		return stream.str();
	}

	[[nodiscard]] std::string MakeUnknownKeyMessage(std::string_view section, std::string_view key)
	{
		std::ostringstream stream;
		stream << "Unknown config key ignored. Section=[" << section << "], Key=" << key;
		return stream.str();
	}

	[[nodiscard]] std::string MakeUnknownSectionMessage(std::string_view section)
	{
		std::ostringstream stream;
		stream << "Unknown config section ignored. Section=[" << section << "]";
		return stream.str();
	}

	[[nodiscard]] std::string_view Trim(std::string_view text) noexcept
	{
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
		{
			text.remove_prefix(1);
		}

		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
		{
			text.remove_suffix(1);
		}

		return text;
	}

	[[nodiscard]] std::string_view RemoveComment(std::string_view text) noexcept
	{
		const std::size_t commentPosition = text.find_first_of("#;");
		if (commentPosition == std::string_view::npos)
		{
			return text;
		}

		return text.substr(0, commentPosition);
	}

	[[nodiscard]] std::string ToLowerCopy(std::string_view text)
	{
		std::string result(text);
		std::ranges::transform(
			result,
			result.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			}
		);

		return result;
	}

	[[nodiscard]] std::optional<unsigned long long> TryParseUnsigned(std::string_view text) noexcept
	{
		text = Trim(text);
		if (text.empty())
		{
			return std::nullopt;
		}

		unsigned long long value = 0;
		const char* begin = text.data();
		const char* end = text.data() + text.size();

		const auto [position, errorCode] = std::from_chars(begin, end, value);
		if (errorCode != std::errc{} || position != end)
		{
			return std::nullopt;
		}

		return value;
	}

	[[nodiscard]] std::optional<long long> TryParseSigned(std::string_view text) noexcept
	{
		text = Trim(text);
		if (text.empty())
		{
			return std::nullopt;
		}

		long long value = 0;
		const char* begin = text.data();
		const char* end = text.data() + text.size();

		const auto [position, errorCode] = std::from_chars(begin, end, value);
		if (errorCode != std::errc{} || position != end)
		{
			return std::nullopt;
		}

		return value;
	}

	[[nodiscard]] std::optional<float> TryParseFloat(std::string_view text)
	{
		text = Trim(text);
		if (text.empty())
		{
			return std::nullopt;
		}

		try
		{
			std::string valueText(text);

			std::size_t processedCount = 0;
			const float value = std::stof(valueText, &processedCount);

			if (processedCount != valueText.size())
			{
				return std::nullopt;
			}

			return value;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	[[nodiscard]] std::optional<bool> TryParseBool(std::string_view text)
	{
		const std::string normalizedText = ToLowerCopy(Trim(text));

		if (normalizedText == "true" || normalizedText == "1" || normalizedText == "yes" || normalizedText == "on")
		{
			return true;
		}

		if (normalizedText == "false" || normalizedText == "0" || normalizedText == "no" || normalizedText == "off")
		{
			return false;
		}

		return std::nullopt;
	}

	[[nodiscard]] std::optional<common::log::LogLevel> TryParseLogLevel(std::string_view value) noexcept
	{
		if (value == "Trace")
		{
			return common::log::LogLevel::Trace;
		}

		if (value == "Debug")
		{
			return common::log::LogLevel::Debug;
		}

		if (value == "Info")
		{
			return common::log::LogLevel::Info;
		}

		if (value == "Warning")
		{
			return common::log::LogLevel::Warning;
		}

		if (value == "Error")
		{
			return common::log::LogLevel::Error;
		}

		return std::nullopt;
	}

	void ApplyNetworkValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = ToLowerCopy(key);

		if (normalizedKey == "port")
		{
			const std::optional<unsigned long long> parsedValue = TryParseUnsigned(value);
			if (parsedValue.has_value()
				&& *parsedValue > 0
				&& *parsedValue <= std::numeric_limits<unsigned short>::max())
			{
				serverConfig.network.port = static_cast<unsigned short>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "workerthreadcount")
		{
			const std::optional<unsigned long long> parsedValue = TryParseUnsigned(value);
			if (parsedValue.has_value())
			{
				serverConfig.network.workerThreadCount = static_cast<std::size_t>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "recvcontextcount")
		{
			const std::optional<unsigned long long> parsedValue = TryParseUnsigned(value);
			if (parsedValue.has_value())
			{
				serverConfig.network.recvContextCount = static_cast<std::size_t>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplySessionValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = ToLowerCopy(key);

		if (normalizedKey == "initialroomid")
		{
			const std::optional<long long> parsedValue = TryParseSigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.session.initialRoomId = static_cast<common::game::RoomId>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "peertimeoutseconds")
		{
			const std::optional<unsigned long long> parsedValue = TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.session.peerTimeout = std::chrono::seconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyTickValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = ToLowerCopy(key);

		if (normalizedKey == "tickintervalms")
		{
			const std::optional<unsigned long long> parsedValue = TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.tick.tickInterval = std::chrono::milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "fixeddeltaseconds")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue > 0.0F)
			{
				serverConfig.tick.fixedDeltaSeconds = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyGameRuleValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = ToLowerCopy(key);

		if (normalizedKey == "initialplayerhp")
		{
			const std::optional<long long> parsedValue = TryParseSigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.gameRule.initialPlayerHp = static_cast<int>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "respawndelayseconds")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue >= 0.0F)
			{
				serverConfig.gameRule.respawnDelaySeconds = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "respawninvincibilityseconds")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue >= 0.0F)
			{
				serverConfig.gameRule.respawnInvincibilitySeconds = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "hitflashdurationseconds")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue >= 0.0F)
			{
				serverConfig.gameRule.hitFlashDurationSeconds = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyBasicWeaponRuleValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = ToLowerCopy(key);

		if (normalizedKey == "bulletdamage")
		{
			const std::optional<long long> parsedValue = TryParseSigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.weaponRule.basicWeaponRule.bulletDamage = static_cast<int>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "bulletspeed")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue > 0.0F)
			{
				serverConfig.weaponRule.basicWeaponRule.bulletSpeed = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "bulletlifeseconds")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue > 0.0F)
			{
				serverConfig.weaponRule.basicWeaponRule.bulletLifeSeconds = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "bulletradius")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue > 0.0F)
			{
				serverConfig.weaponRule.basicWeaponRule.bulletRadius = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "firecooldownseconds")
		{
			const std::optional<float> parsedValue = TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue >= 0.0F)
			{
				serverConfig.weaponRule.basicWeaponRule.fireCooldownSeconds = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyDiagnosticsValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = ToLowerCopy(key);

		if (normalizedKey == "enablestatuslog")
		{
			const std::optional<bool> parsedValue = TryParseBool(value);
			if (parsedValue.has_value())
			{
				serverConfig.diagnostics.enableStatusLog = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "statuslogintervalseconds")
		{
			const std::optional<unsigned long long> parsedValue = TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.diagnostics.statusLogInterval = std::chrono::seconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "loglevel")
		{
			const std::optional<common::log::LogLevel> parsedValue = TryParseLogLevel(value);
			if (parsedValue.has_value())
			{
				serverConfig.diagnostics.logLevel = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "asynclogworkerthreadcount")
		{
			const std::optional<unsigned long long> parsedValue = TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.diagnostics.asyncLogWorkerThreadCount = static_cast<std::size_t>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyConfigValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedSection = ToLowerCopy(section);

		if (normalizedSection == "network")
		{
			ApplyNetworkValue(serverConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "session")
		{
			ApplySessionValue(serverConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "tick")
		{
			ApplyTickValue(serverConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "gamerule")
		{
			ApplyGameRuleValue(serverConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "weapon.basic")
		{
			ApplyBasicWeaponRuleValue(serverConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "diagnostics")
		{
			ApplyDiagnosticsValue(serverConfig, section, key, value, lineNumber, warningList);
			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownSectionMessage(section));
	}
}

namespace server::config
{
	ServerConfigLoadResult ServerConfigLoader::Load(const std::filesystem::path& filePath)
	{
		ServerConfigLoadResult loadResult{};

		std::ifstream file(filePath);
		if (!file.is_open())
		{
			AddWarning(
				loadResult.warningList,
				0,
				"Server config file not found. Default configuration will be used."
			);
			return loadResult;
		}

		loadResult.loadedFromFile = true;

		std::string currentSection;
		std::string line;
		std::size_t lineNumber = 0;

		while (std::getline(file, line))
		{
			++lineNumber;

			std::string_view text = Trim(RemoveComment(line));
			if (text.empty())
			{
				continue;
			}

			if (text.front() == '[' && text.back() == ']')
			{
				text.remove_prefix(1);
				text.remove_suffix(1);

				currentSection = std::string{ Trim(text) };
				continue;
			}

			const std::size_t equalPosition = text.find('=');
			if (equalPosition == std::string_view::npos)
			{
				AddWarning(
					loadResult.warningList,
					lineNumber,
					"Invalid config line ignored. Expected Key=Value format."
				);
				continue;
			}

			const std::string_view key = Trim(text.substr(0, equalPosition));
			const std::string_view value = Trim(text.substr(equalPosition + 1));

			if (currentSection.empty())
			{
				AddWarning(
					loadResult.warningList,
					lineNumber,
					"Config key ignored because it is not inside a section."
				);
				continue;
			}

			if (key.empty())
			{
				AddWarning(loadResult.warningList, lineNumber, "Empty config key ignored.");
				continue;
			}

			ApplyConfigValue(loadResult.config, currentSection, key, value, lineNumber, loadResult.warningList);
		}

		return loadResult;
	}

	ServerConfig ServerConfigLoader::LoadOrDefault(const std::filesystem::path& filePath)
	{
		return Load(filePath).config;
	}
}