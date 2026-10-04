#include "app/Application.hpp"
#include "app/CommandLine.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    pt::ParseResult parsed = pt::parseCommandLine(argc, argv);
    if (!parsed.options) {
        return parsed.exitCode;
    }

    try {
        pt::Application app(*parsed.options);
        return app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return 1;
    }
}
