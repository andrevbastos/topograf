#include "runtime/prt.hpp"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>

namespace prt {

std::optional<std::string> exportPNG(const Grid& g, const Field& f, const std::string& path, int line) {
    try {
        if (!perlin2D::saveNoiseAsPNG(path, f, g.width, g.length)) {
            return std::format("linha {}: erro de execução: não foi possível gravar '{}'", line, path);
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        return std::format("linha {}: erro de execução: {}", line, ex.what());
    }
    return std::nullopt;
}

std::string octavePath(const std::string& dir, const std::string& name, int i) {
    return (std::filesystem::path(dir) / std::format("{}_oitava_{}.png", name, i)).string();
}

std::vector<std::string> exportOctaves(const Grid& g, const perlin2D::NoiseConfig& c,
                                       const std::string& dir, const std::string& name, int line) {
    std::vector<std::string> errors;
    const auto maps = perlin2D::generateOctaveMaps(c);
    for (std::size_t i = 0; i < maps.size(); ++i) {
        if (auto e = exportPNG(g, maps[i], octavePath(dir, name, static_cast<int>(i)), line)) {
            errors.push_back(*e);
        }
    }
    return errors;
}

Options parseOptions(int argc, char** argv) {
    Options o;
    auto usage = [&]() {
        std::cerr << "uso: " << argv[0] << " [--no-display] [--frames N]\n";
        std::exit(2);
    };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-display") {
            o.noDisplay = true;
        } else if (arg == "--frames" && i + 1 < argc) {
            try {
                o.frames = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                usage();
            }
            if (o.frames <= 0) usage();
        } else {
            usage();
        }
    }
    return o;
}

} // namespace prt
