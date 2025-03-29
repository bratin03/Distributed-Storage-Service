#include "dropbox_content_hash.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>\n";
    return 1;
  }

  try {
    std::string hash = dropbox::compute_content_hash(argv[1]);
    std::cout << "Dropbox content hash for '" << argv[1] << "':\n"
              << hash << "\n";
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
