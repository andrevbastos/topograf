#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <latch>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ifcg/ifcg.hpp>
#include <ifcg/graphics/mesh.hpp>

#include "transvoxel/transvoxel.hpp"

using namespace ifcg;

struct Color {
    float r, g, b, a;

    Color operator*(float f) const {
        return {r * f, g * f, b * f, a};
    };
};

namespace mc_detail {

// Dados compartilhados (somente leitura) por todas as fatias.
struct Grid {
    int width, height, depth;
    std::vector<uint> columnHeights;
    float yMin, yRange;
    std::uint64_t keyWidth, keyDepth;
    Color color;
    bool shading;

    std::uint64_t keyZ(std::uint64_t key) const { return (key / keyWidth) % keyDepth; }
};

// Malha de uma fatia de células z ∈ [z0, z1). Índices locais à fatia; `keys`
// (opcional) guarda a chave de aresta de cada vértice para a junção.
struct Slab {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<std::uint64_t> keys;
};

inline Grid prepare(const std::vector<float>& noise, int width, int height, int depth, Color color, bool shading) {
    Grid g {width, height, depth, {}, 0.0f, 1.0f,
            (2ULL * static_cast<std::uint64_t>(width)) - 1ULL,
            (2ULL * static_cast<std::uint64_t>(depth)) - 1ULL,
            color, shading};

    g.columnHeights.resize(static_cast<size_t>(width) * static_cast<size_t>(depth));
    for (uint z = 0; z < static_cast<uint>(depth); ++z) {
        for (uint x = 0; x < static_cast<uint>(width); ++x) {
            const uint noiseIndex = (z * width) + x;
            g.columnHeights[noiseIndex] = static_cast<uint>(std::round(1.0f + noise[noiseIndex] * ((float)height - 1.0f)));
        }
    }

    const auto [minIt, maxIt] = std::minmax_element(g.columnHeights.begin(), g.columnHeights.end());
    g.yMin = static_cast<float>(*minIt);
    g.yRange = std::max(1.0f, static_cast<float>(*maxIt) - g.yMin);
    return g;
}

inline Slab buildSlab(const Grid& g, uint z0, uint z1, bool recordKeys) {
    const Color& color = g.color;
    const std::array<Vertex, 8> corners {
        Vertex{0.0f, 0.0f, 0.0f, color.r, color.g, color.b, color.a},
        Vertex{1.0f, 0.0f, 0.0f, color.r, color.g, color.b, color.a},
        Vertex{0.0f, 1.0f, 0.0f, color.r, color.g, color.b, color.a},
        Vertex{1.0f, 1.0f, 0.0f, color.r, color.g, color.b, color.a},
        Vertex{0.0f, 0.0f, 1.0f, color.r, color.g, color.b, color.a},
        Vertex{1.0f, 0.0f, 1.0f, color.r, color.g, color.b, color.a},
        Vertex{0.0f, 1.0f, 1.0f, color.r, color.g, color.b, color.a},
        Vertex{1.0f, 1.0f, 1.0f, color.r, color.g, color.b, color.a}
    };

    const std::array<uint, 8> cornerDx {0, 1, 0, 1, 0, 1, 0, 1};
    const std::array<uint, 8> cornerDy {0, 0, 1, 1, 0, 0, 1, 1};
    const std::array<uint, 8> cornerDz {0, 0, 0, 0, 1, 1, 1, 1};

    const int width = g.width;
    const int height = g.height;

    auto isVoxelFilled = [&](uint x, uint y, uint z) {
        return y < g.columnHeights[(z * width) + x];
    };

    Slab out;
    auto& vertices = out.vertices;
    auto& indices = out.indices;

    std::unordered_map<std::uint64_t, GLuint> vertexLookup;
    vertexLookup.reserve(static_cast<size_t>(width) * static_cast<size_t>(z1 - z0 + 1));

	for (uint z = z0; z < z1; ++z) {
		for (uint y = 0; y < static_cast<uint>(height); ++y) {
			for (uint x = 0; x < static_cast<uint>(width - 1); ++x) {
				uint caseIndex = 0;

				for (int i = 0; i < 8; ++i) {
					if (isVoxelFilled(x + cornerDx[i], y + cornerDy[i], z + cornerDz[i])) {
						caseIndex |= (1 << i);
					}
				}

				if (caseIndex != 0 && caseIndex != 255) {
					auto classIndex = regularCellClass[caseIndex];
					auto cellData = regularCellData[classIndex];
					auto vertexCount = cellData.GetVertexCount();
					auto triangleCount = cellData.GetTriangleCount();

                    std::array<GLuint, 12> cellVertexIndices {};

					for (int i = 0; i < vertexCount; i++) {
						auto edgeInfo = regularVertexData[caseIndex][i];
						auto lowByte = edgeInfo & 0xFF;
						auto a = lowByte >> 4;
						auto b = lowByte & 0x0F;

                        const std::uint64_t keyX = (2ULL * x) + cornerDx[a] + cornerDx[b];
                        const std::uint64_t keyY = (2ULL * y) + cornerDy[a] + cornerDy[b];
                        const std::uint64_t keyZ = (2ULL * z) + cornerDz[a] + cornerDz[b];
                        const std::uint64_t key = ((keyY * g.keyDepth) + keyZ) * g.keyWidth + keyX;

                        auto existingVertex = vertexLookup.find(key);
                        if (existingVertex != vertexLookup.end()) {
                            cellVertexIndices[i] = existingVertex->second;
                            continue;
                        }

                        float dim = 1.0f;
                        if (g.shading) {
                            const float t = std::clamp((static_cast<float>(y) - g.yMin) / g.yRange, 0.0f, 1.0f);
                            dim = 0.35f + 0.65f * t;
                        }

						auto pos = corners[a] % corners[b];
						pos = (pos + Vertex{(float)x, (float)y, (float)z, 0.0f, 0.0f, 0.0f, 0.0f}) * Vertex{1.0f, 1.0f, 1.0f, dim, dim, dim, 1.0f};

                        const GLuint vertexIndex = static_cast<GLuint>(vertices.size());
						vertices.push_back(pos);
                        if (recordKeys) out.keys.push_back(key);
                        vertexLookup.emplace(key, vertexIndex);
                        cellVertexIndices[i] = vertexIndex;
					}
					for (int i = 0; i < (triangleCount * 3); i++) {
						indices.push_back(cellVertexIndices[cellData.vertexIndex[i]]);
					}
				}
			}
		}
	}
    return out;
}

} // namespace mc_detail

// Malha de superfície de um mapa de altura 2D (width × depth, valores em [0, 1])
// por Marching Cubes. Cada coluna recebe round(1 + h·(height − 1)) voxels.
// Com shading = true, a cor é escurecida pela altura (dim = 0.35 + 0.65·t).
inline std::pair<std::vector<Vertex>, std::vector<GLuint>> getMarchingCubeData(
    const std::vector<float>& noise,
    int width,
    int height,
    int depth,
    Color color = {1.0f, 1.0f, 1.0f, 1.0f},
    bool shading = true
) {
    if (width < 2 || height < 2 || depth < 2 || noise.size() < static_cast<size_t>(width) * static_cast<size_t>(depth)) {
        return {{}, {}};
    }

    const auto g = mc_detail::prepare(noise, width, height, depth, color, shading);
    auto slab = mc_detail::buildSlab(g, 0, static_cast<uint>(depth - 1), false);

	if (!slab.vertices.empty() && !slab.indices.empty()) {
        return {std::move(slab.vertices), std::move(slab.indices)};
    }
    return {{}, {}};
}

// Mesma malha de getMarchingCubeData (mesmos vértices, na mesma ordem), calculada
// em fatias de z nas workers de `tm`. Bloqueia até todas as fatias terminarem.
//
// Cada fatia deduplica seus vértices localmente. Os vértices do plano entre duas
// fatias aparecem nas duas; na junção, a fatia de cima reaproveita os da fatia de
// baixo, que na versão sequencial seriam criados primeiro (ordem z, y, x).
inline std::pair<std::vector<Vertex>, std::vector<GLuint>> getMarchingCubeDataParallel(
    TaskMaster& tm,
    const std::vector<float>& noise,
    int width,
    int height,
    int depth,
    Color color = {1.0f, 1.0f, 1.0f, 1.0f},
    bool shading = true
) {
    if (width < 2 || height < 2 || depth < 2 || noise.size() < static_cast<size_t>(width) * static_cast<size_t>(depth)) {
        return {{}, {}};
    }

    const auto g = mc_detail::prepare(noise, width, height, depth, color, shading);

    const uint cells = static_cast<uint>(depth - 1);
    const uint hw = std::max(1u, std::thread::hardware_concurrency());
    const uint slabCount = std::min(cells, 4 * hw);

    std::vector<uint> bounds(slabCount + 1);
    for (uint k = 0; k <= slabCount; ++k) {
        bounds[k] = static_cast<uint>((static_cast<std::uint64_t>(cells) * k) / slabCount);
    }

    std::vector<mc_detail::Slab> slabs(slabCount);
    std::vector<std::exception_ptr> errors(slabCount);
    std::latch done(slabCount);
    for (uint k = 0; k < slabCount; ++k) {
        tm.addTask([&, k]() {
            try {
                slabs[k] = mc_detail::buildSlab(g, bounds[k], bounds[k + 1], true);
            } catch (...) {
                errors[k] = std::current_exception();
            }
            done.count_down();
        });
    }
    done.wait();
    for (const auto& e : errors) {
        if (e) std::rethrow_exception(e);
    }

    // Junção sequencial, na ordem de z.
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::size_t totalVertices = 0, totalIndices = 0;
    for (const auto& s : slabs) {
        totalVertices += s.vertices.size();
        totalIndices += s.indices.size();
    }
    vertices.reserve(totalVertices);
    indices.reserve(totalIndices);

    std::unordered_map<std::uint64_t, GLuint> lowerPlane;  // plano z = bounds[k] → índice global
    std::vector<GLuint> remap;
    for (uint k = 0; k < slabCount; ++k) {
        auto& s = slabs[k];
        const std::uint64_t lowZ = 2ULL * bounds[k];
        const std::uint64_t highZ = 2ULL * bounds[k + 1];

        std::unordered_map<std::uint64_t, GLuint> upperPlane;
        remap.assign(s.vertices.size(), 0);
        for (std::size_t i = 0; i < s.vertices.size(); ++i) {
            const std::uint64_t key = s.keys[i];
            const std::uint64_t kz = g.keyZ(key);
            if (k > 0 && kz == lowZ) {
                remap[i] = lowerPlane.at(key);
            } else {
                remap[i] = static_cast<GLuint>(vertices.size());
                vertices.push_back(s.vertices[i]);
            }
            if (kz == highZ) upperPlane.emplace(key, remap[i]);
        }
        for (GLuint idx : s.indices) indices.push_back(remap[idx]);

        lowerPlane = std::move(upperPlane);
        s = {};  // libera a fatia
    }

	if (!vertices.empty() && !indices.empty()) {
        return {std::move(vertices), std::move(indices)};
    }
    return {{}, {}};
}
