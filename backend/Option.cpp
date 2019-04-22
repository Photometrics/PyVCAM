#include "Option.h"

/* System */
#include <sstream>

/* Local */
#include "Log.h"
#include "Utils.h"

const std::string pm::Option::ArgValueSeparator = "=";
const char pm::Option::ValuesSeparator = ',';
const char pm::Option::ValueGroupsSeparator = ';';

pm::Option::Option(const std::vector<std::string>& names,
            const std::vector<std::string>& argsDescs,
            const std::vector<std::string>& defVals,
            const std::string& desc, uint32_t id, const Handler& handler)
    : m_names(names),
    m_argsDescs(argsDescs),
    m_defVals(defVals),
    m_desc(desc),
    m_id(id),
    m_valueType(ValueType::Custom),
    m_handler(handler)
{
    if (m_argsDescs.empty())
    {
        m_valueType = ValueType::None;
    }
    else if (m_argsDescs.size() == 1 && m_argsDescs.at(0).empty())
    {
        m_valueType = ValueType::Boolean;
    }
    // Otherwise value type is always custom
}

bool pm::Option::IsMatching(const std::string& nameWithValue) const
{
    const std::string::size_type argValSepPos =
        nameWithValue.find(ArgValueSeparator);
    for (const std::string& name : m_names)
    {
        // Whole name (without value) has to match
        if (name == nameWithValue.substr(0, argValSepPos))
            return true;
    }
    return false;
}

bool pm::Option::RunHandler(const std::string& nameWithValue) const
{
    if (!m_handler)
        return false;

    const std::string::size_type argValSepPos =
        nameWithValue.find(ArgValueSeparator);
    const std::string name = nameWithValue.substr(0, argValSepPos);
    const std::string value = (argValSepPos == std::string::npos)
        ? "" : nameWithValue.substr(argValSepPos + 1);

    switch (m_valueType)
    {
    case ValueType::Custom:
        // ArgValueSeparator has to follow the name
        if (argValSepPos == std::string::npos)
        {
            Log::LogE("Option " + name + " requires a value");
            return false;
        }
        break;
    case ValueType::Boolean:
        // ArgValueSeparator following the name is optional
        if (argValSepPos != std::string::npos)
        {
            // But if found the value has to be valid
            bool boolValue;
            if (!StrToBool(value, boolValue))
            {
                Log::LogE("Option " + name
                        + " requires a boolean value or no value separator");
                return false;
            }
        }
        break;
    case ValueType::None:
        // ArgValueSeparator must not follow the name
        if (argValSepPos != std::string::npos)
        {
            Log::LogE("Option " + name + " does not take any value");
            return false;
        }
        break;
    }

    const bool ok = m_handler(value);
    std::string msg = "Handler for option " + name
        + " was called with value '" + value + "' - ";
    if (ok)
    {
        msg += "OK";
        Log::LogI(msg);
    }
    else
    {
        msg += "ERROR";
        Log::LogE(msg);
    }

    return ok;
}

bool operator==(const pm::Option& lhs, const pm::Option& rhs)
{
    if (lhs.GetId() != rhs.GetId())
        return false;
    if (lhs.GetNames().size() != rhs.GetNames().size())
        return false;
    for (size_t n = 0; n < lhs.GetNames().size(); n++)
        if (lhs.GetNames()[n] != rhs.GetNames()[n])
            return false;
    return true;
}
