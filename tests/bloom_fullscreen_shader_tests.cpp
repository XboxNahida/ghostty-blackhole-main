#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "Unable to read " << path.string() << '\n';
        std::exit(2);
    }

    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

}  // namespace

int main() {
    const std::filesystem::path sourceRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path();
    const std::string bloomSource =
        ReadFile(sourceRoot / "src" / "bloom_renderer.cpp");

    // The oversized fullscreen triangle uses p values from 0 to 2 at its
    // vertices. Across the visible NDC rectangle, interpolation already maps
    // p to 0..1. Multiplying by 0.5 samples only half of the scene texture and
    // stretches it across the screen.
    if (bloomSource.find("uv = p;") == std::string::npos) {
        std::cerr
            << "Bloom fullscreen pass must use interpolated p directly so "
               "the visible viewport samples UV 0..1.\n";
        return 1;
    }

    if (bloomSource.find("uv = p * 0.5;") != std::string::npos) {
        std::cerr
            << "Bloom fullscreen pass still halves UVs and will magnify the "
               "desktop texture.\n";
        return 1;
    }

    return 0;
}
