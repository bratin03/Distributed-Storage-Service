#include "authentication.hpp"
#include "../logger/Mylogger.h"   // For MyLogger
#include "jwt-cpp/jwt.h"        // jwt-cpp
#include "../initiation/initiation.hpp"       // For Initiation::public_key
#include <algorithm>            // std::count

namespace Authentication
{
    std::optional<std::string> verify_jwt(const std::string& token)
    {
        MyLogger::debug("Inside verify_jwt");

        if (token.empty())
        {
            MyLogger::error("JWT Format Error: Empty token");
            return std::nullopt;
        }

        if (std::count(token.begin(), token.end(), '.') != 2)
        {
            MyLogger::error("JWT Format Error: Incorrect token structure");
            return std::nullopt;
        }

        MyLogger::debug("JWT Format Correct");

        try
        {
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::rs256(Initiation::public_key))
                .with_issuer("auth-server");

            verifier.verify(decoded);
            return decoded.get_payload_claim("userID").as_string();
        }
        catch (const std::system_error& e)
        {
            MyLogger::error("JWT Verification Failed: " + std::string(e.what()));
            return std::nullopt;
        }
        catch (const std::exception& e)
        {
            MyLogger::error("JWT Verification Failed: " + std::string(e.what()));
            return std::nullopt;
        }
    }

    bool authenticate_request(const httplib::Request& req, httplib::Response& res, std::string& userID)
    {
        MyLogger::debug("Inside authenticate_request");

        if (!req.has_header("Authorization"))
        {
            res.status = 401;
            res.set_content(R"({"error": "Missing authentication token"})", "application/json");
            MyLogger::error("Authentication failed: Missing token");
            return false;
        }

        MyLogger::debug("Authorization header found");

        std::string token = req.get_header_value("Authorization");
        if (token.rfind("Bearer ", 0) == 0)
        {
            MyLogger::debug("Bearer found");
            token = token.substr(7); // Remove "Bearer " prefix
        }

        MyLogger::debug("Token trimmed: " + token);

        auto verified_user = verify_jwt(token);
        if (!verified_user)
        {
            res.status = 403;
            res.set_content(R"({"error": "Invalid token"})", "application/json");
            MyLogger::error("Authentication failed: Invalid token");
            return false;
        }

        userID = *verified_user;
        MyLogger::info("Authenticated user: " + userID);
        return true;
    }
}
