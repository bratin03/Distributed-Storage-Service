#include "dropbox_content_hash.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " <filename>\n";
    return 1;
  }

  try
  {
    // Open the file in binary mode
    std::ifstream file(argv[1], std::ios::binary);
    if (!file)
    {
      throw std::runtime_error("Error opening file: " + std::string(argv[1]));
    }

    // Read file content into a string
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    // Compute the Dropbox content hash using the file content.
    std::string hash = dropbox::compute_content_hash(content);
    std::cout << "Dropbox content hash for '" << argv[1] << "':\n"
              << hash << "\n";
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
