#include <iostream>
#include <vector>
#include "client_lib/kv.hpp" // Make sure this header is in your include path

int main()
{
    // Define a list of server addresses.
    std::vector<std::string> servers = {
        "http://127.0.0.1:5000",
        "http://127.0.0.1:5001",
        "http://127.0.0.1:5002",
        "http://127.0.0.1:5003",
        "http://127.0.0.1:5004"};

    // Set a key "example" with value "Hello World"
    std::cout << "Setting key 'example' to 'Hello World'..." << std::endl;
    Response setResp = set(servers, "example", "Hello World");
    if (setResp.success)
    {
        std::cout << "Set succeeded: " << setResp.value << std::endl;
    }
    else
    {
        std::cerr << "Set failed: " << setResp.err << std::endl;
    }

    // Get the key "example"
    std::cout << "Getting key 'example'..." << std::endl;
    Response getResp = get(servers, "example");
    if (getResp.success)
    {
        std::cout << "Get succeeded: " << getResp.value << std::endl;
    }
    else
    {
        std::cerr << "Get failed: " << getResp.err << std::endl;
    }

    // Delete the key "example"
    std::cout << "Deleting key 'example'..." << std::endl;
    Response delResp = del(servers, "example");
    if (delResp.success)
    {
        std::cout << "Delete succeeded: " << delResp.value << std::endl;
    }
    else
    {
        std::cerr << "Delete failed: " << delResp.err << std::endl;
    }

    // Try to get the key "example" after deletion
    std::cout << "Getting key 'example' after deletion..." << std::endl;
    Response getAfterDel = get(servers, "example");
    if (getAfterDel.success)
    {
        std::cout << "Get succeeded: " << getAfterDel.value << std::endl;
    }
    else
    {
        std::cerr << "Get failed: " << getAfterDel.err << std::endl;
    }

    return 0;
}
