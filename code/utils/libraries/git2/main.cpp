#include "three_way_merge.hpp"
#include <iostream>

int main() {
  std::string base = "Line 1\nLine 2\nLine 3\n";
  std::string local = "Line 1\nLine 2 - local change\nLine 3\n";
  std::string remote = "Line 1\nLine 2\nLine 3\n";
  std::string merged;

  if (MergeLib::three_way_merge(base, local, remote, merged)) {
    std::cout << "Merge successful:\n" << merged;
  } else {
    std::cout << "Merge conflict detected. Manual resolution required.\n";
  }
  return 0;
}
