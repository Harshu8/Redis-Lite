#include "Utilities.hpp"

#include <algorithm>
#include <cctype>

namespace redis_lite::utils {

std::string trim(const std::string& input) {
    const auto start = input.find_first_not_of(" \n\r\t");
    if (start == std::string::npos) {
        return "";
    }

    const auto end = input.find_last_not_of(" \n\r\t");
    return input.substr(start, end - start + 1);
}

std::string toUpper(std::string input) {
    std::transform(input.begin(), input.end(), input.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return input;
}

} // namespace redis_lite::utils
