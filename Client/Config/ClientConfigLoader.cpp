#include "ClientConfigLoader.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <Common/Config/ConfigText.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Config/ClientTransportType.h>

#include "ClientConfigValidator.h"

namespace
{
	using WarningList = std::vector<client::config::ClientConfigWarning>;

	void AddWarning(WarningList& warningList, std::size_t lineNumber, std::string message)
	{
		client::config::ClientConfigWarning warning{};
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

	[[nodiscard]] std::optional<client::config::ClientTransportType> TryParseClientTransportType(std::string_view value)
	{
		const std::string normalizedValue = common::config::ToLowerCopy(common::config::Trim(value));

		if (normalizedValue == "socket")
		{
			return client::config::ClientTransportType::Socket;
		}

		if (normalizedValue == "iocp")
		{
			return client::config::ClientTransportType::Iocp;
		}

		return std::nullopt;
	}

	void ApplyNetworkValue(
		client::config::ClientConfig& clientConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "serverip")
		{
			value = common::config::Trim(value);
			if (!value.empty())
			{
				clientConfig.network.serverIp = std::string(value);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "serverport")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value()
				&& *parsedValue > 0
				&& *parsedValue <= std::numeric_limits<unsigned short>::max())
			{
				clientConfig.network.serverPort = static_cast<unsigned short>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "transporttype")
		{
			const std::optional<client::config::ClientTransportType> parsedValue = TryParseClientTransportType(value);
			if (parsedValue.has_value())
			{
				clientConfig.network.transportType = *parsedValue;
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "iocpworkerthreadcount")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.network.iocpWorkerThreadCount = static_cast<std::size_t>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "iocprecvcontextcount")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.network.iocpRecvContextCount = static_cast<std::size_t>(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyAccountValue(
		client::config::ClientConfig& clientConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "loginname")
		{
			clientConfig.account.loginName
				= std::string(common::config::Trim(value));

			return;
		}

		if (normalizedKey == "passwordhash")
		{
			clientConfig.account.passwordHash
				= std::string(common::config::Trim(value));

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyTimingValue(
		client::config::ClientConfig& clientConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "updatesleepms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.timing.updateSleepInterval = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "accountloginretryms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);

			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.timing.accountLoginRetryInterval = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "joinretryms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.timing.joinRetryInterval = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "keepalivems")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.timing.keepAliveInterval = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "roomjoinms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.timing.roomJoinInterval = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "interpolationadjuststepms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.timing.interpolationAdjustStep = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplyInterpolationValue(
		client::config::ClientConfig& clientConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "defaultdelayms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value())
			{
				clientConfig.interpolation.defaultDelay = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "mindelayms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value())
			{
				clientConfig.interpolation.minDelay = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "maxdelayms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value())
			{
				clientConfig.interpolation.maxDelay = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplySnapshotValue(
		client::config::ClientConfig& clientConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "assemblytimeoutms")
		{
			const std::optional<unsigned long long> parsedValue = common::config::TryParseUnsigned(value);
			if (parsedValue.has_value() && *parsedValue > 0)
			{
				clientConfig.snapshot.assemblyTimeout = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownKeyMessage(section, key));
	}

	void ApplySimulationValue(
		client::config::ClientConfig& clientConfig,
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
				clientConfig.simulation.tickInterval = common::time::Milliseconds(*parsedValue);
			}
			else
			{
				AddWarning(warningList, lineNumber, MakeInvalidValueMessage(section, key, value));
			}

			return;
		}

		if (normalizedKey == "deltaseconds")
		{
			const std::optional<float> parsedValue = common::config::TryParseFloat(value);
			if (parsedValue.has_value() && *parsedValue > 0.0F)
			{
				clientConfig.simulation.deltaSeconds = *parsedValue;
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
		client::config::ClientConfig& clientConfig,
		std::string_view section,
		std::string_view key,
		std::string_view value,
		std::size_t lineNumber,
		WarningList& warningList
	)
	{
		const std::string normalizedKey = common::config::ToLowerCopy(key);

		if (normalizedKey == "loglevel")
		{
			const std::optional<common::log::LogLevel> parsedValue =
				common::log::TryParseLogLevel(value);

			if (parsedValue.has_value())
			{
				clientConfig.diagnostics.logLevel = *parsedValue;
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
				clientConfig.diagnostics.asyncLogWorkerThreadCount = static_cast<std::size_t>(*parsedValue);
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
		client::config::ClientConfig& clientConfig,
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
			ApplyNetworkValue(clientConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "account")
		{
			ApplyAccountValue(clientConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "timing")
		{
			ApplyTimingValue(clientConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "interpolation")
		{
			ApplyInterpolationValue(clientConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "snapshot")
		{
			ApplySnapshotValue(clientConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "simulation")
		{
			ApplySimulationValue(clientConfig, section, key, value, lineNumber, warningList);
			return;
		}

		if (normalizedSection == "diagnostics")
		{
			ApplyDiagnosticsValue(clientConfig, section, key, value, lineNumber, warningList);
			return;
		}

		AddWarning(warningList, lineNumber, MakeUnknownSectionMessage(section));
	}
}

namespace client::config
{
	ClientConfigLoadResult ClientConfigLoader::Load(const std::filesystem::path& filePath)
	{
		ClientConfigLoadResult loadResult{};

		std::ifstream file(filePath);
		if (!file.is_open())
		{
			AddWarning(
				loadResult.warningList,
				0,
				"Client config file not found. Default configuration will be used."
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

	ClientConfigLoadResult ClientConfigLoader::LoadValidated(const std::filesystem::path& filePath)
	{
		ClientConfigLoadResult loadResult = Load(filePath);

		std::vector<ClientConfigWarning> validationWarningList = ClientConfigValidator::ValidateAndNormalize(loadResult.config);

		loadResult.warningList.insert(
			loadResult.warningList.end(),
			std::make_move_iterator(validationWarningList.begin()),
			std::make_move_iterator(validationWarningList.end())
		);
		return loadResult;
	}

	ClientConfig ClientConfigLoader::LoadOrDefault(const std::filesystem::path& filePath)
	{
		return LoadValidated(filePath).config;
	}
}