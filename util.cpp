#include "util.h"
#include <sstream>
#include <cmath>
std::vector<std::string> splitPath(const std::string& path) {
  char delimiter = '/';
  std::vector<std::string> result;
  std::istringstream iss(path);
  std::string token;

  while (std::getline(iss, token, delimiter)) {
    result.push_back(token);
  }

  return result;
}

std::tuple<std::string, std::string> splitPathParent(const std::string& path) {
    char delimiter = '/';
  std::string::size_type pos = path.find_last_of(delimiter);
  if (pos == std::string::npos) {
    return std::make_tuple("", path);
  }
  return std::make_tuple(path.substr(0, pos), path.substr(pos + 1));
}
bool isPowerOf(int num, int base) {
  double result = log(num) / log(base);
  return (result - (int)result) == 0;
}