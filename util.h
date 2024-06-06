#include <string>
#include <vector>
#include <tuple>

std::vector<std::string> splitPath(const std::string& path);
std::tuple<std::string, std::string> splitPathParent(const std::string& path);
bool isPowerOf(int num, int base);