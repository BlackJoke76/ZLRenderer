#include "zl/app/Application.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

std::uint32_t parseMaxFrames(int argc, char** argv)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view(argv[i]) == "--frames") {
            return static_cast<std::uint32_t>(std::stoul(argv[i + 1]));
        }
    }

    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        zl::app::Application application;
        application.run(parseMaxFrames(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
