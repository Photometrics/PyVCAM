#pragma once
#ifndef _OPTION_H
#define _OPTION_H

/* System */
#include <functional>
#include <vector>
#include <cstdint>
#include <string>

namespace pm {

class Option
{
public:
    using HandlerProto = bool(const std::string&);
    using Handler = std::function<HandlerProto>;

public:
    static const std::string ArgValueSeparator;
    static const char ValuesSeparator;
    static const char ValueGroupsSeparator;

public:
    enum class ValueType {
        // In case the argument descriptions vector is empty (no ArgValueSeparator)
        None,
        // Option requires ArgValueSeparator
        Custom,
        // In case the argument descriptions vector has one empty string
        Boolean
    };

public:
    // Size of argsDescs and defVals have to be equal.
    // The id is unique ID of related PVCAM parameter (PARAM_*).
    // Values with zeroed highest byte (from 1 to 0x00FFFFFF) can be used for
    // non-PVCAM options. Zero value should stay reserved.
    Option(const std::vector<std::string>& names,
            const std::vector<std::string>& argsDescs,
            const std::vector<std::string>& defVals,
            const std::string& desc, uint32_t id, const Handler& handler);
    ~Option()
    {}

    Option() = delete;

    // Used for printing the usage of the option object
    const std::vector<std::string>& GetNames() const
    { return m_names; }

    // Used for printing the usage of the option object
    const std::vector<std::string>& GetArgsDescriptions() const
    { return m_argsDescs; }

    // Used for printing the usage of the option object
    const std::vector<std::string>& GetDefaultValues() const
    { return m_defVals; }

    // Used for printing the usage of the option object
    const std::string& GetDescription() const
    { return m_desc; }

    // Used for accessing PVCAM parameter or easy Option recognition
    uint32_t GetId() const
    { return m_id; }

    // For boolean value type the ArgValueSeparator and value might be missing
    ValueType GetValueType() const
    { return m_valueType; }

    // Determine if runtime option starts with option name
    bool IsMatching(const std::string& nameWithValue) const;

    // Execute the function pointer associated with the option
    bool RunHandler(const std::string& nameWithValue) const;

private:
    std::vector<std::string> m_names;
    std::vector<std::string> m_argsDescs;
    std::vector<std::string> m_defVals;
    std::string m_desc;
    uint32_t m_id;
    ValueType m_valueType;
    Handler m_handler;
};

} // namespace pm

// Compared two options, names have to be the same in exact order
bool operator==(const pm::Option& lhs, const pm::Option& rhs);

#endif /* _OPTION_H */
