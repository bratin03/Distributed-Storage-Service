/*
    How to use the authentication_server:

    1. Install Dependencies:
       - Ensure you have the required libraries installed:
         - `cpp-httplib`
         - `jwt-cpp`
         - `nlohmann/json`




    2. Generate RSA Keys:

    NOTE: the keys should be in PKCS format

          - Run the following commands to generate a private and public RSA key pair:

           # Generate a private key (2048-bit RSA)
           openssl genpkey -algorithm RSA -out private.pem -pkeyopt rsa_keygen_bits:2048

           # Extract the corresponding public key
           openssl pkey -in private.pem -pubout -out public.pem

          - Place `private.pem` in the same directory as the executable or update the file path in `loadKey()`.

          - The given commands generate in the correct format but if you face any issues ensure the private key is in the correct format and accessible by the server.




    3. Compile the server:
       - Use a C++ compiler with C++17 support.
       - Example compilation command (adjust paths as needed):
         ```
         g++ -std=c++17 -o auth_server authentication_server.cpp -I../../utils/libraries/jwt-cpp/include -I../../utils/libraries/cpp-httplib -I/usr/include/nlohmann -L/usr/lib -lssl -lcrypto -lpthread
         ```





    4. Run the server:
         - Execute the compiled binary:
         ```
         ./auth_server
         ```



    5. Make a login request:
       - Use `curl` or Postman to send a POST request to authenticate.
       - Example request using `curl`:
         ```
         curl -X POST http://localhost:8080/login -H "Content-Type: application/json" -d '{"userID":"admin", "password":"password123"}'
         ```
       - Expected response (if credentials are correct):
         ```json
         {
             "status": "success",
             "token": "<JWT_TOKEN_HERE>"
         }
         ```
       - If credentials are incorrect:
         ```json
         {
             "status": "error",
             "message": "Invalid credentials"
         }
         ```


*/

#include "../../utils/libraries/cpp-httplib/httplib.h"         // HTTP library
#include "../../utils/libraries/jwt-cpp/include/jwt-cpp/jwt.h" // JWT library
#include <nlohmann/json.hpp>                                   // JSON parsing
#include "./logger/Mylogger.h"                                 // Custom logger
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

using json = nlohmann::json;
using namespace httplib;

// Hardcoded user database
std::unordered_map<std::string, std::string> userDB = {
    {"admin", "password123"},
    {"user1", "pass1234"}};

// Load RSA Private Key
std::string loadKey(const std::string &filename)
{

    MyLogger::info("Loading private key from file: " + filename);
    std::ifstream file(filename, std::ios::in);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open key file: " + filename);
    }
    std::string key((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (key.empty())
    {
        throw std::runtime_error("Key file is empty: " + filename);
    }
    return key;

    // Debugging: Log first few characters to verify loading
    // MyLogger::debug("Loaded key: " + key.substr(0, 30) + "...");
    // return key;
}

// Load keys
const std::string PRIVATE_KEY = loadKey("private.pem");

// Function to generate JWT token using RS256
std::string generateJWT(const std::string &userID)
{

    MyLogger::info("Generating JWT token");

    auto token = jwt::create()
                     .set_issuer("auth-server")
                     .set_type("JWT")
                     .set_subject(userID)
                     .set_payload_claim("userID", jwt::claim(userID))
                     .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(30))
                     .sign(jwt::algorithm::rs256("", PRIVATE_KEY, "", ""));
    /*
        there was a problem with the private key, so I used "" for the public key and "" for the secret key
        The rs256 constructor has multiple overloads, and using only PRIVATE_KEY assumes it's both the private and public key. However:
        RSA256 (RS256) uses asymmetric cryptography, meaning it requires a private key for signing and a public key for verification
    */
    // MyLogger::debug("Generated token for user");

    return token;
}

// Authentication handler
json authenticateUser(const std::string &userID, const std::string &password)
{
    if (userDB.find(userID) != userDB.end() && userDB[userID] == password)
    {

        MyLogger::debug("Inside authenticateUser valid user");
        return {{"status", "success"}, {"token", generateJWT(userID)}};
    }
    else
    {

        MyLogger::debug("Inside authenticateUser invalid user");
        return {{"status", "error"}, {"message", "Invalid credentials"}};
    }
}

int main()
{
    httplib::Server svr;

    svr.Post("/login", [](const httplib::Request &req, httplib::Response &res)
             {
        
        MyLogger::info("Received login request");
        
        try {
            auto body = json::parse(req.body);
            if (!body.contains("userID") || !body.contains("password")) {
                res.status = 400;
                res.set_content(R"({"status": "error", "message": "Invalid JSON format"})", "application/json");
                return;
            }

            std::string userID = body["userID"];
            std::string password = body["password"];
            json response = authenticateUser(userID, password);
            
            MyLogger::info("Authentication response: " + response.dump());
            
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {

            MyLogger::error("Error processing request: " + std::string(e.what()));

            res.status = 500;
            res.set_content(R"({"status": "error", "message": "Server error"})", "application/json");
        } });

    MyLogger::info("Authentication Server running on port 8080...");
    svr.listen("0.0.0.0", 8080);

    return 0;
}
