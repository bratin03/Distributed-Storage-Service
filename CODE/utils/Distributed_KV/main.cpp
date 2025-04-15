#include <iostream>
#include <vector>
#include <fstream> // Include this header for std::ifstream
#include "client_lib/kv.hpp" // Make sure this header is in your include path
#include <nlohmann/json_fwd.hpp>
using namespace distributed_KV; 
using json = nlohmann::json;

int main()
{
    // Define a list of server addresses.
    std::vector<std::string> servers;
    std::ifstream ipFile("ip_list.txt");
    if (!ipFile)
    {
        std::cerr << "Failed to open ip_list.txt" << std::endl;
        return 1;
    }
    std::string ip;
    while (std::getline(ipFile, ip))
    {
        if (!ip.empty())
        {
            servers.push_back(ip);
        }
    }
    ipFile.close();

    // Set a key "example" with value "Hello World"
    std::cout << "Setting key 'example' to 'value1'..." << std::endl;
    json value = {{"version_number", "1"}, {"data", "11111111111111"}};
    Response setResp = set(servers, "example", value.dump());
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
        try
        {
            json jsonResponse = json::parse(getResp.value);
            std::cout << "Get succeeded: " << jsonResponse.dump(4) << std::endl; // Pretty print with 4 spaces
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to parse response as JSON: " << e.what() << std::endl;
        }
    }
    else
    {
        std::cerr << "Get failed: " << getResp.err << std::endl;
    }
    

    // Set a key "example" with value "Hello World"
    std::cout << "Setting key 'example' to 'value2'..." << std::endl;
    value = {{"version_number", "1"}, {"data", "2222222222222"}};
    setResp = set(servers, "example", value.dump());
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
    getResp = get(servers, "example");
    if (getResp.success)
    {
        try
        {
            json jsonResponse = json::parse(getResp.value);
            std::cout << "Get succeeded: " << jsonResponse.dump(4) << std::endl; // Pretty print with 4 spaces
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to parse response as JSON: " << e.what() << std::endl;
        }
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
