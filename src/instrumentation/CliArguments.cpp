#include "instrumentation/CliArguments.hpp"

namespace nxsan {

NxsResult<CliArguments, std::string> CliArguments::Parse(int argc, char** argv) {
    CliArguments out;

    // We start at 1 here to ignore the executable at argv[0].
    for (int i = 1; i < argc; i++) {
        out.m_inputFiles.push_back(argv[i]);
    }

    if (out.m_inputFiles.size() == 0) {
        return std::string("No input files.");
    }

    return out;
}

}  // namespace nxsan
