#pragma once

#include <optional>

#include <Common/Net/SessionToken.h>

namespace server::service
{
	[[nodiscard]] std::optional<common::net::SessionToken> GenerateSessionToken() noexcept;
}