#include <iostream>
#include <jwt-cpp/jwt.h>
#include <string>

int main() {
	// Create a JWT token using HS256 signing algorithm.
	auto token = jwt::create()
					 .set_issuer("auth0")
					 .set_payload_claim("sample", jwt::claim(std::string("test")))
					 .sign(jwt::algorithm::hs256{"secret"});

	std::cout << "Generated Token:\n" << token << "\n";

	// Decode the token.
	auto decoded = jwt::decode(token);

	std::cout << "\nPayload Claims:\n";
	for (auto& e : decoded.get_payload_json()) {
		std::cout << e.first << " : " << e.second << "\n";
	}

	// Verify the token.
	auto verifier = jwt::verify()
						.with_issuer("auth0")
						.with_claim("sample", jwt::claim(std::string("test")))
						.allow_algorithm(jwt::algorithm::hs256{"secret"});

	try {
		verifier.verify(decoded);
		std::cout << "\nToken verification succeeded.\n";
	} catch (const std::exception& e) { std::cerr << "Token verification failed: " << e.what() << "\n"; }

	return 0;
}
