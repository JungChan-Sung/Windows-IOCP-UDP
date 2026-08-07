#pragma once

#include <optional>

#include <Common/Net/SessionToken.h>

namespace server::net
{
	[[nodiscard]] std::optional<common::net::SessionToken> GenerateSessionToken() noexcept;
}