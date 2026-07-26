#include "PasswordHashVerifier.h"

#include <algorithm>
#include <cstddef>

namespace server::account
{
	bool PasswordHashVerifier::Verify(std::string_view providedPasswordHash, std::string_view storedPasswordHash) const noexcept
	{
		const std::size_t comparisonLength = std::max(providedPasswordHash.size(), storedPasswordHash.size());
		std::size_t difference = providedPasswordHash.size() ^ storedPasswordHash.size();
		for (std::size_t index = 0; index < comparisonLength; ++index)
		{
			const unsigned char providedByte =  (index < providedPasswordHash.size()) ? static_cast<unsigned char>(providedPasswordHash[index]) : 0;
			const unsigned char storedByte = (index < storedPasswordHash.size()) ? static_cast<unsigned char>(storedPasswordHash[index]) : 0;
			difference |= providedByte ^ storedByte;
		}

		return difference == 0;
	}
}