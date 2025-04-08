#pragma once
#include <string>
#include <optional>
#include <httplib.h>

namespace Authentication
{
    // Verifies a JWT token and extracts the userID if valid
    std::optional<std::string> verify_jwt(const std::string& token);

    // Middleware-style function that authenticates incoming request
    bool authenticate_request(const httplib::Request& req, httplib::Response& res, std::string& userID);
}
