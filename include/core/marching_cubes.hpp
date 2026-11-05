#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

    std::vector<uint> columnHeights(static_cast<size_t>(width) * static_cast<size_t>(depth));
    for (uint z = 0; z < static_cast<uint>(depth); ++z) {
        for (uint x = 0; x < static_cast<uint>(width); ++x) {
            const uint noiseIndex = (z * width) + x;
            columnHeights[noiseIndex] = static_cast<uint>(std::round(1.0f + noise[noiseIndex] * ((float)height - 1.0f)));
        }
    }

    const auto [minIt, maxIt] = std::minmax_element(columnHeights.begin(), columnHeights.end());
    const float yMin = static_cast<float>(*minIt);
    const float yRange = std::max(1.0f, static_cast<float>(*maxIt) - yMin);

    auto isVoxelFilled = [&](uint x, uint y, uint z) {
        return y < columnHeights[(z * width) + x];
    };

	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;

    const std::uint64_t keyWidth = (2ULL * static_cast<std::uint64_t>(width)) - 1ULL;
    const std::uint64_t keyDepth = (2ULL * static_cast<std::uint64_t>(depth)) - 1ULL;
    std::unordered_map<std::uint64_t, GLuint> vertexLookup;
    vertexLookup.reserve(static_cast<size_t>(width) * static_cast<size_t>(depth));

	for (uint z = 0; z < static_cast<uint>(depth - 1); ++z) {
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
                        const std::uint64_t key = ((keyY * keyDepth) + keyZ) * keyWidth + keyX;

                        auto existingVertex = vertexLookup.find(key);
                        if (existingVertex != vertexLookup.end()) {
                            cellVertexIndices[i] = existingVertex->second;
                            continue;
                        }

                        float dim = 1.0f;
                        if (shading) {
                            const float t = std::clamp((static_cast<float>(y) - yMin) / yRange, 0.0f, 1.0f);
                            dim = 0.35f + 0.65f * t;
                        }

						auto pos = corners[a] % corners[b];
						pos = (pos + Vertex{(float)x, (float)y, (float)z, 0.0f, 0.0f, 0.0f, 0.0f}) * Vertex{1.0f, 1.0f, 1.0f, dim, dim, dim, 1.0f};

                        const GLuint vertexIndex = static_cast<GLuint>(vertices.size());
						vertices.push_back(pos);
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

	if (!vertices.empty() && !indices.empty()) {
        return {vertices, indices};
    }
    return {{}, {}};
};
