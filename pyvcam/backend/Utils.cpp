#include "Utils.h"

/* System */
#include <algorithm>
#include <map>
#include <sstream>

// Local
#include "Log.h"

bool pm::StrToBool(const std::string& str, bool& value)
{
    if (str.empty())
        return false;

    std::string tmpStr = str;
    // Without ICU library we can only assume the string contains ASCII chars only
    std::transform(tmpStr.begin(), tmpStr.end(), tmpStr.begin(), ::tolower);

    static const std::map<std::string, bool> validValues = {
        { "0",      false },
        { "false",  false },
        { "off",    false },
        { "no",     false },
        { "1",      true },
        { "true",   true },
        { "on",     true },
        { "yes",    true }
    };

    if (validValues.count(tmpStr) == 0)
        return false;

    value = validValues.at(tmpStr);
    return true;
}

std::vector<std::string> pm::SplitString(const std::string& string, char delimiter)
{
    std::vector<std::string> items;
    std::istringstream ss(string);
    std::string item;
    while (std::getline(ss, item, delimiter))
        items.push_back(item);
    return items;
}

std::string pm::JoinStrings(const std::vector<std::string>& strings, char delimiter)
{
    std::string string;
    for (const std::string& item : strings)
        string += item + delimiter;
    if (!strings.empty())
        string.pop_back(); // Remove delimiter at the end
    return string;
}
