/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#pragma once
#include <httplib.h>

#include <algorithm>  // std::count
#include <optional>
#include <string>

#include "../initiation/initiation.hpp"  // For Initiation::public_key
#include "Mylogger.hpp"          // For MyLogger
#include "jwt-cpp/jwt.h"                 // jwt-cpp

namespace Authentication {
// Verifies a JWT token and extracts the userID if valid
std::optional<std::string> verify_jwt(const std::string &token);

// Middleware-style function that authenticates incoming request
bool authenticate_request(const httplib::Request &req, httplib::Response &res, std::string &userID);
}  // namespace Authentication
