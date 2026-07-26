#include "PasswordHashVerifierTests.h"

#include <Server/Account/PasswordHashVerifier.h>

#include <Tests/DebugTestResult.h>

namespace tests::server
{
	DebugTestResult RunPasswordHashVerifierTests()
	{
		DebugTestResult result{};

		::server::account::PasswordHashVerifier passwordHashVerifier;

		tests::Expect(
			result,
			passwordHashVerifier.Verify(
				"password_hash",
				"password_hash"
			),
			"PasswordHashVerifier: accept matching hashes"
		);

		tests::Expect(
			result,
			!passwordHashVerifier.Verify(
				"password_hash",
				"different_hash"
			),
			"PasswordHashVerifier: reject different hashes"
		);

		tests::Expect(
			result,
			!passwordHashVerifier.Verify(
				"short",
				"shorter"
			),
			"PasswordHashVerifier: reject different lengths"
		);

		tests::Expect(
			result,
			!passwordHashVerifier.Verify(
				"Password_Hash",
				"password_hash"
			),
			"PasswordHashVerifier: compare hashes case sensitively"
		);

		return result;
	}
}