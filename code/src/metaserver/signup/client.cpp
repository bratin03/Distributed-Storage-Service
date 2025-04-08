#include <iostream>
#define CPPHTTPLIB_HEADER_ONLY
#include "../../../utils/libraries/cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main()
{
    // Use the IP address of the load balancer
    httplib::Client cli("127.0.0.1", 8080);

    // Construct the JSON payload for signup
    json payload;
    payload["username"] = "lordu";
    payload["password"] = "testpassword"; // In production, this should be a hashed password

    // Send the POST request to the /signup endpoint
    auto res = cli.Post("/signup", payload.dump(), "application/json");

    // Check and print the result
    if (res)
    {
        if (res->status == 200)
        {
            std::cout << "Signup successful: " << res->body << std::endl;
        }
        else
        {
            std::cout << "Signup failed (" << res->status << "): " << res->body << std::endl;
        }
    }
    else
    {
        std::cerr << "Error: No response from server" << std::endl;
    }

    return 0;
}
