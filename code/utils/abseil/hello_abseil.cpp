#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"

int main() {
  std::vector<std::string> words = {"Hello", "Abseil", "World"};
  std::string joined = absl::StrJoin(words, " ");
  std::cout << "Joined string: " << joined << std::endl;
  return 0;
}
