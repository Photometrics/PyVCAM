#pragma once
#ifndef _OPTION_CONTROLLER_H
#define _OPTION_CONTROLLER_H

/* System */
#include <vector>
#include <string>

/* Local */
#include "Option.h"

namespace pm {

class OptionController
{
public:
    OptionController();

    // Add new unique option
    bool AddOption(const Option& option);

    // Process parameters and execute the command line options
    bool ProcessOptions(int argc, char* argv[]);

    // Get string with CLI options usage
    std::string GetOptionsDescription(const std::vector<Option>& options) const;

    // Registered options
    const std::vector<Option>& GetOptions() const
    { return m_options; }

    // Registered options
    const std::vector<Option>& GetAllProcessedOptions() const
    { return m_optionsPassed; }

    // Registered options
    const std::vector<Option>& GetFailedProcessedOptions() const
    { return m_optionsPassedFailed; }

private:
    // Verify that there are no duplicate options registered
    bool VerifyOptions(std::string& conflictingName) const;

private:
    std::vector<Option> m_options;
    std::vector<Option> m_optionsPassed;
    std::vector<Option> m_optionsPassedFailed;
};

} // namespace pm

#endif /* _OPTION_CONTROLLER_H */
