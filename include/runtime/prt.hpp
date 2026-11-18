#pragma once

// Runtime do topograf: usado pelo interpretador e pelos executáveis gerados pelo
// compilador (`topograf prog.topo -o prog`). Não inclui o ifcg; o display fica
// em topograf_display.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/noise_gen.hpp"

namespace prt {

// Grade width × length em ordem de linha (índice = j * width + i), valores em [0, 1].
using Field = std::vector<float>;

struct Grid {
    int width = 0;
    int length = 0;
    int height = 0;  // 0 se o cabeçalho não define height

    std::size_t size() const { return static_cast<std::size_t>(width) * static_cast<std::size_t>(length); }
};

// ---------- operações ponto a ponto (seção 4 do handoff), com clamp em [0, 1] ----------

inline float c01(float v) { return std::clamp(v, 0.0f, 1.0f); }

inline float add(float a, float b) { return c01(a + b); }
inline float sub(float a, float b) { return c01(a - b); }
inline float scale(float k, float a) { return c01(k * a); }
inline float max(float a, float b) { return c01(std::max(a, b)); }
inline float min(float a, float b) { return c01(std::min(a, b)); }
inline float above(float a, float b) { return c01(a > b ? a : 0.0f); }
inline float below(float a, float b) { return c01(a < b ? a : 0.0f); }
inline float avg(float a, float b) { return c01((a + b) / 2.0f); }
inline float mix(float a, float b, float na, float nb) { return c01((a * na + b * nb) / (na + nb)); }

// ---------- primitivos ----------

// NoiseConfig com a grade do cabeçalho e os padrões nos demais campos.
inline perlin2D::NoiseConfig config(const Grid& g) {
    perlin2D::NoiseConfig c {};
    c.width = g.width;
    c.height = g.length;  // NoiseConfig.height = length da DSL
    return c;
}

// ---------- export ----------

// Mensagens de erro já começam com "linha N: erro de execução: ...".
std::optional<std::string> exportPNG(const Grid& g, const Field& f, const std::string& path, int line);
std::vector<std::string> exportOctaves(const Grid& g, const perlin2D::NoiseConfig& c,
                                       const std::string& dir, const std::string& name, int line);
std::string octavePath(const std::string& dir, const std::string& name, int i);

// ---------- display (implementado em topograf_display, que liga o ifcg) ----------

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
};

struct Layer {
    std::function<Field()> field;  // avaliado no processo da janela
    std::optional<Rgb> color;
};

struct DisplaySpec {
    int line = 0;
    std::vector<Layer> layers;
    bool depth = false;
};

// Uma janela por display, todas ao mesmo tempo, uma por processo (D15).
// frames > 0 fecha cada janela após esse número de frames. Retorna 0 se tudo deu certo.
int runDisplays(const Grid& g, const std::vector<DisplaySpec>& displays, int frames);

// ---------- linha de comando dos executáveis gerados ----------

struct Options {
    bool noDisplay = false;
    int frames = 0;
};

// Aceita --no-display e --frames N. Encerra o processo com uso em caso de erro.
Options parseOptions(int argc, char** argv);

} // namespace prt
