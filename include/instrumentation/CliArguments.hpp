#pragma once

#include <string>
#include <vector>

#include "utils/NxsResult.hpp"

namespace nxsan {

// Class for handling CLI arguments passed to the NXSAN
// instrumentation tool.
class CliArguments {
public:
    // Parses command line arguments into a CLI arguments structure.
    static NxsResult<CliArguments, std::string> Parse(int argc, char** argv);

    // Returns input files configured for conversion.
    const std::vector<std::string>& GetInputFiles() const { return m_inputFiles; }

private:
    std::vector<std::string> m_inputFiles;
};

}  // namespace nxsan
