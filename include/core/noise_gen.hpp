#pragma once

#include <iostream>
#include <math.h>
#include <random>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cerrno>
#include <cstring>

#include "stb_image.h"
#include "stb_image_write.h"

namespace fs = std::filesystem;

namespace perlin2D {
    struct Vector2D {
        float x, y;
    };

    struct NoiseConfig {
        int width, height;
        int wave = 250;
        float freq = 4.0f;
        float lac = 2.0f;
        float amp = 1.0f;
        float pers = 0.5f;
        float exp = 1.0f;
        unsigned int seed = 0;
        unsigned int octaves = 3;
    };

    inline Vector2D randomGradient(int ix, int iy, unsigned int seed) {
        const unsigned w {8 * sizeof(unsigned)};
        const unsigned s {w / 2}; 
        unsigned a = ix, b = iy;
        a ^= seed;
        a *= 3284157443;
    
        b ^= a << s | a >> w - s;
        b *= 1911520717;
    
        a ^= b << s | b >> w - s;
        a *= 2048419325;
        float random = a * (3.14159265 / ~(~0u >> 1));
        
        Vector2D v;
        v.x = sin(random);
        v.y = cos(random);
    
        return v;
    }

    inline float dotGridGradient(int ix, int iy, float x, float y, unsigned int seed) {
        auto gradient {randomGradient(ix, iy, seed)};

        float dx {x - (float)ix};
        float dy {y - (float)iy};

        return (dx * gradient.x + dy * gradient.y);
    }

    inline float interpolate(float a0, float a1, float w) {
        return (a1 - a0) * ((w * (w * 6.0f - 15.0f) + 10.0f) * w * w * w) + a0;
    }

    inline float perlin(float x, float y, unsigned int seed) {
        int x0 {(int)x};
        int x1 {x0 + 1};
        int y0 {(int)y};
        int y1 {y0 + 1};

        float sx {x - (float)x0};
        float sy {y - (float)y0};

        float n0 {dotGridGradient(x0, y0, x, y, seed)};
        float n1 {dotGridGradient(x1, y0, x, y, seed)};
        float ix0 {interpolate(n0, n1, sx)};

        n0 = dotGridGradient(x0, y1, x, y, seed);
        n1 = dotGridGradient(x1, y1, x, y, seed);
        float ix1 {interpolate(n0, n1, sx)};

        float result {interpolate(ix0, ix1, sy)};
        
        return result;
    }

    inline std::vector<float> generateNoiseMap(NoiseConfig config) {
        std::vector<float> noiseMap(config.width * config.height);

        std::mt19937 rng {config.seed};
        std::uniform_real_distribution<float> dist {0.0f, 1000.0f};
        std::vector<Vector2D> offsets(config.octaves);
        for (auto& o : offsets) o = {dist(rng), dist(rng)};

        for (int y{0}; y < config.height; ++y) {
            for (int x{0}; x < config.width; ++x) {
                int index {y * config.width + x};

                float nx {static_cast<float>(x) / config.wave};
                float ny {static_cast<float>(y) / config.wave};

                float val {0.0f};
                float ampSum {0.0f};
                float freq {config.freq};
                float lac {config.lac};
                float amp {1.0f};
                float pers {config.pers};

                for (unsigned int i{0}; i < config.octaves; i++) {
                    unsigned int octaveSeed {config.seed + i * 1013u};
                    val += perlin(nx * freq + offsets[i].x, ny * freq + offsets[i].y, octaveSeed) * amp;
                    ampSum += amp * amp;
                    amp *= pers;
                    freq *= lac;
                }

                val = val / std::sqrt(ampSum) * config.amp;

                if (val > 1.0f) val = 1.0f;
                else if (val < -1.0f) val = -1.0f;
                
                val = (val + 1.0f) * 0.5f;
                val = pow(val, config.exp);
                
                noiseMap[index] = val;
            }
        }

        return noiseMap;
    }

    // Uma camada por oitava, com os mesmos offsets e frequências de generateNoiseMap.
    // Cada camada é normalizada isoladamente (sem o peso de persistência).
    inline std::vector<std::vector<float>> generateOctaveMaps(NoiseConfig config) {
        std::vector<std::vector<float>> octaveMaps(config.octaves, std::vector<float>(config.width * config.height));

        std::mt19937 rng {config.seed};
        std::uniform_real_distribution<float> dist {0.0f, 1000.0f};
        std::vector<Vector2D> offsets(config.octaves);
        for (auto& o : offsets) o = {dist(rng), dist(rng)};

        for (int y{0}; y < config.height; ++y) {
            for (int x{0}; x < config.width; ++x) {
                int index {y * config.width + x};

                float nx {static_cast<float>(x) / config.wave};
                float ny {static_cast<float>(y) / config.wave};

                float freq {config.freq};

                for (unsigned int i{0}; i < config.octaves; i++) {
                    unsigned int octaveSeed {config.seed + i * 1013u};
                    float val {perlin(nx * freq + offsets[i].x, ny * freq + offsets[i].y, octaveSeed) * config.amp};
                    freq *= config.lac;

                    if (val > 1.0f) val = 1.0f;
                    else if (val < -1.0f) val = -1.0f;

                    val = (val + 1.0f) * 0.5f;
                    octaveMaps[i][index] = pow(val, config.exp);
                }
            }
        }

        return octaveMaps;
    }

    // Retorna false se o PNG não pôde ser gravado. create_directories pode lançar filesystem_error.
    inline bool saveNoiseAsPNG(const std::string& filename, const std::vector<float>& noiseMap, int width, int height) {
        std::vector<unsigned char> imageData(width * height);
        
        for (size_t i = 0; i < noiseMap.size(); ++i) {
            imageData[i] = static_cast<unsigned char>(noiseMap[i] * 255);
        }

        const fs::path parent {fs::path(filename).parent_path()};
        if (!parent.empty()) fs::create_directories(parent);
        if (!stbi_write_png(filename.c_str(), width, height, 1, imageData.data(), width)) {
            std::cerr << "Failed to save noise map as PNG: " << filename << " (" << std::strerror(errno) << ")" << std::endl;
            return false;
        }
        return true;
    }
}
