#include "lang/display.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <ifcg/ifcg.hpp>
#include <ifcg/graphics/mesh.hpp>

#include "core/marching_cubes.hpp"

namespace fs = std::filesystem;

namespace display {

namespace {

// Deslocamento vertical por camada contra z-fighting (D19).
constexpr float LAYER_OFFSET = 0.05f;

const Color GRAY {0.5f, 0.5f, 0.5f, 1.0f};

Color toColor(const std::optional<ast::Rgb>& rgb) {
    if (!rgb) return GRAY;
    return {rgb->r / 255.0f, rgb->g / 255.0f, rgb->b / 255.0f, 1.0f};
}

// Corpo do processo filho: uma janela para um `display`. Retorna o código de saída.
int showOne(const ast::Display& d, rt::Evaluator& ev, const fs::path& exeDir, int frames) {
    const int W = ev.width();
    const int L = ev.length();
    const int H = ev.height().value_or(0);  // T11 garante height

    // Malhas de cada camada (CPU), antes de abrir a janela (D1, D8).
    std::vector<std::pair<std::vector<Vertex>, std::vector<GLuint>>> layers;
    for (std::size_t i = 0; i < d.layers.size(); ++i) {
        const rt::Field field = ev.evaluate(*d.layers[i].expr);
        auto data = getMarchingCubeData(field, W, H, L, toColor(d.layers[i].color), d.depth);
        if (data.first.empty()) {
            std::cerr << std::format("display linha {}: camada {} sem malha "
                                     "(width, length e height precisam ser >= 2)\n", d.loc.line, i + 1);
        }
        layers.push_back(std::move(data));
    }

    // O ifcg carrega os shaders de "../resources/..." relativo ao cwd (D20).
    std::error_code ec;
    fs::current_path(exeDir / "resources", ec);
    if (ec) {
        std::cerr << std::format("display linha {}: erro de execução: recursos do ifcg não encontrados em '{}'\n",
                                 d.loc.line, (exeDir / "resources").string());
        return 1;
    }

    const std::string title = std::format("perlang — display linha {}", d.loc.line);
    Engine::init(1200, 800, title.c_str());
    Engine::setup3D();

    auto& renderer = Engine::getRenderer();
    auto& input = Engine::getInputHandler();
    auto& camera = renderer.getCamera();

    const float span = static_cast<float>(std::max(W, L));
    const glm::vec3 pos {-0.2f * W, H + 0.5f * span, -0.2f * L};
    const glm::vec3 center {W / 2.0f, H / 2.0f, L / 2.0f};
    camera.setPosition(pos);
    camera.setOrientation(glm::normalize(center - pos));
    renderer.setFarPlane(4.0f * std::max(span, static_cast<float>(H)));
    camera.setSpeed(1.0f);

    input.addKeyCallback(Key::SHIFT_L, KeyAction::HELD, [&camera]() { camera.setSpeed(3.0f); });
    input.addKeyCallback(Key::SHIFT_L, KeyAction::RELEASE, [&camera]() { camera.setSpeed(1.0f); });

    std::vector<std::shared_ptr<MeshBase>> meshes;
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].first.empty()) continue;
        auto mesh = std::make_shared<Mesh>(std::move(layers[i].first), std::move(layers[i].second),
                                           renderer.getShaderID(), GL_TRIANGLES);
        mesh->translate(0.0f, static_cast<float>(i) * LAYER_OFFSET, 0.0f);
        renderer.addMesh(mesh);
        meshes.push_back(std::move(mesh));
    }

    int frame = 0;
    Engine::loop(LoopConfig {
        .afterRender = [&]() {
            if (frames > 0 && ++frame >= frames) {
                glfwSetWindowShouldClose(Engine::getWindow().getGLFWwindow(), true);
            }
        },
    });

    // As malhas têm VAO/VBO: destruir antes de encerrar o contexto.
    for (auto& m : meshes) renderer.removeMesh(m);
    meshes.clear();
    Engine::terminate();
    return 0;
}

} // namespace

int runDisplays(const ast::Program& program, rt::Evaluator& ev, const fs::path& exeDir, int frames) {
    std::vector<std::pair<pid_t, int>> children;  // pid, linha do display
    int status = 0;

    for (const auto& cmd : program.commands) {
        const auto* d = std::get_if<ast::Display>(&cmd);
        if (!d) continue;

        std::fflush(stdout);
        std::fflush(stderr);
        std::cout.flush();
        std::cerr.flush();

        const pid_t pid = fork();
        if (pid < 0) {
            std::cerr << std::format("display linha {}: erro de execução: fork falhou ({})\n",
                                     d->loc.line, std::strerror(errno));
            status = 1;
            continue;
        }
        if (pid == 0) {
            int rc = 1;
            try {
                rc = showOne(*d, ev, exeDir, frames);
            } catch (const std::exception& ex) {
                std::cerr << std::format("display linha {}: erro de execução: {}\n", d->loc.line, ex.what());
            }
            std::cout.flush();
            std::cerr.flush();
            _exit(rc);  // não roda destrutores estáticos herdados do pai
        }
        children.emplace_back(pid, d->loc.line);
    }

    for (const auto& [pid, line] : children) {
        int ws = 0;
        while (waitpid(pid, &ws, 0) < 0 && errno == EINTR) {}
        if (WIFEXITED(ws) && WEXITSTATUS(ws) == 0) continue;

        status = 1;
        if (WIFSIGNALED(ws)) {
            std::cerr << std::format("display linha {}: janela encerrada pelo sinal {}\n", line, WTERMSIG(ws));
        } else if (WIFEXITED(ws)) {
            std::cerr << std::format("display linha {}: janela terminou com código {}\n", line, WEXITSTATUS(ws));
        }
    }
    return status;
}

} // namespace display
