/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/
#include <regex>
#include <string>
int main() {
  const std::string str = "test0159";
  std::regex re;
  re = std::regex("^[a-z]+[0-9]+$",
       std::regex_constants::extended | std::regex_constants::nosubs);
  return std::regex_search(str, re) ? 0 : -1;
}
