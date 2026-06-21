#include "ServerConfigLoader.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <Common/Config/ConfigText.h>

#include "ServerConfigValidator.h"

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

	[[nodiscard]] std::optional<common::log::LogLevel> TryParseLogLevel(std::string_view value) noexcept
	{
		const std::string normalizedValue = common::config::ToLowerCopy(common::config::Trim(value));

		if (normalizedValue == "trace")
		{
			return common::log::LogLevel::Trace;
		}

		if (normalizedValue == "debug")
		{
			return common::log::LogLevel::Debug;
		}

		if (normalizedValue == "info")
		{
			return common::log::LogLevel::Info;
		}

		if (normalizedValue == "warning" || normalizedValue == "warn")
		{
			return common::log::LogLevel::Warning;
		}

		if (normalizedValue == "error")
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
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "port")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
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
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
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
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
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
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "initialroomid")
		{
			const std::optional<long long> parsedValue = common::config::TryParseSigned(value);
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
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
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
	
	void ApplyReliableUdpValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "maxpendingpacketcount")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.reliableUdp.maxPendingPacketCount = static_cast<std::size_t>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "maxresendcount")
		{
			const std::optional<long long> parsedValue = common::config::TryParseSigned(value);
			if (parsedValue.has_value() && *parsedValue >= 0)
			{
				serverConfig.reliableUdp.maxResendCount = static_cast<int>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "resendintervalms" || normalizedKey == "resendintervalmilliseconds")
		{
			const std::optional<common::time::Milliseconds> parsedValue = common::config::TryParseMilliseconds(value);
			if (parsedValue.has_value() && *parsedValue > common::time::Milliseconds::zero())
			{
				serverConfig.reliableUdp.resendInterval = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyUdpFaultSimulationValue(
		server::config::ServerConfig& serverConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "enabled")
		{
			const std::optional<bool> parsedValue = common::config::TryParseBool(value);

			if (parsedValue.has_value())
			{
				serverConfig.udpFaultSimulation.enabled = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "droprate")
		{
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);

			if (parsedValue.has_value() && *parsedValue >= 0.0F && *parsedValue <= 1.0F)
			{
				serverConfig.udpFaultSimulation.dropRate = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "duplicaterate")
		{
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);

			if (parsedValue.has_value() && *parsedValue >= 0.0F && *parsedValue <= 1.0F)
			{
				serverConfig.udpFaultSimulation.duplicateRate = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "reorderrate")
		{
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);

			if (parsedValue.has_value() && *parsedValue >= 0.0F && *parsedValue <= 1.0F)
			{
				serverConfig.udpFaultSimulation.reorderRate = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "mindelayms" || normalizedKey == "mindelaymilliseconds")
		{
			const std::optional<common::time::Milliseconds> parsedValue = common::config::TryParseMilliseconds(value);

			if (parsedValue.has_value())
			{
				serverConfig.udpFaultSimulation.minDelay = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "maxdelayms" || normalizedKey == "maxdelaymilliseconds")
		{
			const std::optional<common::time::Milliseconds> parsedValue = common::config::TryParseMilliseconds(value);

			if (parsedValue.has_value())
			{
				serverConfig.udpFaultSimulation.maxDelay = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "reorderdelayms" || normalizedKey == "reorderdelaymilliseconds")
		{
			const std::optional<common::time::Milliseconds> parsedValue = common::config::TryParseMilliseconds(value);

			if (parsedValue.has_value())
			{
				serverConfig.udpFaultSimulation.reorderDelay = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "randomseed")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);

			if (parsedValue.has_value() && *parsedValue <= std::numeric_limits<std::uint32_t>::max())
			{
				serverConfig.udpFaultSimulation.randomSeed = static_cast<std::uint32_t>(*parsedValue);
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
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "tickintervalms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				serverConfig.tick.tickInterval = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "fixeddeltaseconds")
		{
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "initialplayerhp")
		{
			const std::optional<long long> parsedValue = common::config::TryParseSigned(value);
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
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "bulletdamage")
		{
			const std::optional<long long> parsedValue = common::config::TryParseSigned(value);
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
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
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
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "enablestatuslog")
		{
			const std::optional<bool> parsedValue = common::config::TryParseBool(value);
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
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
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
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
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
		const std::string normalizedSection = common::config::ToLowerCopy(section);

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

		if (normalizedSection == "reliableudp")
		{
			ApplyReliableUdpValue(serverConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "udpfaultsimulation")
		{
			ApplyUdpFaultSimulationValue(
				serverConfig,
				section,
				key,
				value,
				lineNumber,
				warningList
			);

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

			std::string_view text = common::config::Trim(common::config::RemoveComment(line));
			if (text.empty())
			{
				continue;
			}

			if (text.front() == '[' && text.back() == ']')
			{
				text.remove_prefix(1);
				text.remove_suffix(1);

				currentSection = std::string{ common::config::Trim(text) };
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

			const std::string_view key = common::config::Trim(text.substr(0, equalPosition));
			const std::string_view value = common::config::Trim(text.substr(equalPosition + 1));

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

	ServerConfigLoadResult ServerConfigLoader::LoadValidated(const std::filesystem::path& filePath)
	{
		ServerConfigLoadResult loadResult = Load(filePath);

		std::vector<ServerConfigWarning> validationWarningList = ServerConfigValidator::ValidateAndNormalize(loadResult.config);

		loadResult.warningList.insert(
			loadResult.warningList.end(),
			std::make_move_iterator(validationWarningList.begin()),
			std::make_move_iterator(validationWarningList.end())
		);
		return loadResult;
	}

	ServerConfig ServerConfigLoader::LoadOrDefault(const std::filesystem::path& filePath)
	{
		return LoadValidated(filePath).config;
	}
}