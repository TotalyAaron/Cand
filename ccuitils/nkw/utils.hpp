#ifndef __UTILS_HPP_CA__
#define __UTILS_HPP_CA__
#include <regex>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
std::string removeComments(const std::string& code);
// Function that accepts a vector<string>, for use
std::vector<std::string> removeComments(const std::vector<std::string>& lines) {
    std::vector<std::string> cleaned;
    cleaned.reserve(lines.size());
    for (const auto& line : lines) {
        cleaned.push_back(removeComments(line));
    }
    return cleaned;
}
std::vector<std::string> extractAllLinesFromIfstream(std::ifstream& file) {
    std::vector<std::string> allLines;
    std::string line;
    while (std::getline(file, line)) {
        allLines.push_back(line);
    }
    return allLines;
}
std::string ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r\f\v");
    return (start == std::string::npos) ? "" : s.substr(start);
}
std::string rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\n\r\f\v");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
std::string trim(const std::string& s) {
    return ltrim(rtrim(s));
}
#endif