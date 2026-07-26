#pragma once

#include <string_view>

namespace server::account
{
	class PasswordHashVerifier final
	{
	public:
		PasswordHashVerifier() = default;
		~PasswordHashVerifier() noexcept = default;

		PasswordHashVerifier(const PasswordHashVerifier&) = delete;
		PasswordHashVerifier& operator=(const PasswordHashVerifier&) = delete;

		PasswordHashVerifier(PasswordHashVerifier&&) = delete;
		PasswordHashVerifier& operator=(PasswordHashVerifier&&) = delete;

	public:
		[[nodiscard]] bool Verify(std::string_view providedPasswordHash, std::string_view storedPasswordHash) const noexcept;
	};
}