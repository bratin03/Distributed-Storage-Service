#include <folly/FBString.h>
#include <iostream>

int main()
{
    // Construct an fbstring from a C-string literal
    folly::fbstring myString("Hello, fbstring!");

    // Use fbstring methods similar to std::string
    std::cout << "The string is: " << myString << std::endl;
    std::cout << "Size: " << myString.size() << std::endl;

    // Concatenation works too
    myString += " How are you?";
    std::cout << "After concatenation: " << myString << std::endl;
    std::cout << "Size: " << myString.size() << std::endl;

    return 0;
}
