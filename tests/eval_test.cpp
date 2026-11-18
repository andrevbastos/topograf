// Testes da avaliação (RF-A) e das exportações (RF-O1, O2), sem janela.
// uso: eval_test <dir de avaliacao/> <dir temporário>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <string>

#include "lang/driver.hpp"
#include "lang/eval.hpp"
#include "lang/semantic.hpp"
#include "stb_image.h"

namespace fs = std::filesystem;

static int failures = 0;

static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "ok    " : "FALHA ") << what << "\n";
    if (!ok) ++failures;
}

struct Loaded {
    Driver driver;
    sema::Result sem;
};

static bool load(Loaded& l, const std::string& path) {
    if (!l.driver.parse(path)) {
        std::cout << "FALHA parse " << path << ": " << l.driver.error << "\n";
        ++failures;
        return false;
    }
    l.sem = sema::analyze(l.driver.program);
    for (const auto& e : l.sem.errors) std::cout << "FALHA " << sema::formatError(e) << "\n";
    failures += static_cast<int>(l.sem.errors.size());
    return l.sem.ok();
}

static void tabela(const std::string& dir) {
    Loaded l;
    if (!load(l, dir + "/tabela.topo")) return;
    rt::Evaluator ev(l.driver.program, l.sem);
    ev.preload("A", {0.2f, 0.8f});
    ev.preload("B", {0.5f, 0.5f});

    const std::map<std::string, std::pair<float, float>> expected {
        {"soma",   {0.7f, 1.0f}},
        {"sub",    {0.0f, 0.3f}},
        {"maior",  {0.5f, 0.8f}},
        {"menor",  {0.2f, 0.5f}},
        {"sobre",  {0.0f, 0.8f}},
        {"sob",    {0.2f, 0.0f}},
        {"med",    {0.35f, 0.65f}},
        {"mis",    {0.41f, 0.59f}},
        {"mis2",   {0.41f, 0.59f}},
        {"mis11",  {0.35f, 0.65f}},
        {"sobB",   {0.5f, 0.0f}},
        {"esc",    {0.4f, 1.0f}},
        {"esc2",   {0.1f, 0.4f}},
        {"limiar", {0.0f, 0.8f}},
        {"mais",   {0.7f, 1.0f}},
        {"menos",  {0.1f, 0.0f}},
        {"aninh",  {0.2f, 0.5f}},
        {"usa",    {0.7f, 0.7f}},
        {"const",  {1.0f, 1.0f}},
    };
    for (const auto& [name, v] : expected) {
        const auto& f = ev.field(name);
        const bool ok = f.size() == 2 && std::abs(f[0] - v.first) < 1e-6f && std::abs(f[1] - v.second) < 1e-6f;
        check(ok, std::format("tabela {:7} = [{}, {}] (esperado [{}, {}])",
                              name, f.size() > 0 ? f[0] : -1, f.size() > 1 ? f[1] : -1, v.first, v.second));
    }
    check(ev.primitiveGenerations() == 0, "tabela: A e B pré-carregados não chamam o gerador");
}

static void cache(const std::string& dir) {
    Loaded l;
    if (!load(l, dir + "/cache.topo")) return;
    rt::Evaluator ev(l.driver.program, l.sem);

    const auto& d = ev.field("D");
    check(d.size() == 16 * 8, "cache: campo com width × length pontos");
    check(ev.primitiveGenerations() == 1, std::format("cache: A gerado uma vez (A1), gerado {}x", ev.primitiveGenerations()));

    // Parâmetros chegam ao gerador como em D7.
    perlin2D::NoiseConfig c {};
    c.width = 16; c.height = 8; c.freq = 2.0f; c.octaves = 2; c.seed = 3; c.pers = 0.7f;
    check(ev.field("A") == perlin2D::generateNoiseMap(c), "cache: A = generateNoiseMap(frequency 2, octaves 2, seed 3, persistence 0.7)");

    perlin2D::NoiseConfig cp {};
    cp.width = 16; cp.height = 8; cp.lac = 3.0f; cp.amp = 1.5f; cp.exp = 2.0f; cp.wave = 100; cp.seed = 5;
    check(ev.field("P") == perlin2D::generateNoiseMap(cp), "cache: P = generateNoiseMap(lac 3, amp 1.5, exp 2, wave 100, seed 5)");

    perlin2D::NoiseConfig padrao {};
    padrao.width = 16; padrao.height = 8; padrao.seed = 5;
    check(ev.field("P") != perlin2D::generateNoiseMap(padrao), "cache: lacunarity/amplitude/exponent/wave alteram o campo");

    bool inRange = true;
    for (float v : d) inRange = inRange && v >= 0.0f && v <= 1.0f;
    check(inRange, "cache: valores de D em [0, 1]");
}

static bool pngEquals(const std::string& path, const rt::Field& f, int w, int h) {
    int pw = 0, ph = 0, ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &pw, &ph, &ch, 0);
    if (!data) return false;
    bool ok = pw == w && ph == h && ch == 1;
    for (int i = 0; ok && i < w * h; ++i) ok = data[i] == static_cast<unsigned char>(f[i] * 255);
    stbi_image_free(data);
    return ok;
}

static void png(const std::string& dir, const std::string& tmp) {
    Loaded l;
    if (!load(l, fs::absolute(dir + "/png.topo").string())) return;
    rt::Evaluator ev(l.driver.program, l.sem);

    fs::remove_all(tmp);
    fs::create_directories(tmp);
    const auto oldCwd = fs::current_path();
    fs::current_path(tmp);

    const auto errors = rt::runExports(l.driver.program, ev);
    for (const auto& e : errors) std::cout << "  " << e << "\n";
    check(errors.empty(), "png: exportações sem erro");

    check(pngEquals("m.png", rt::Field(12, 0.5f), 4, 3), "png: m.png sem diretório, 4×3 cinza, valor 127 (O1)");
    check(pngEquals("sub/dir/a.png", ev.field("A"), 4, 3), "png: sub/dir/a.png cria diretórios e bate com o campo");

    const auto& decl = std::get<ast::PrimitiveDecl>(l.driver.program.commands[1]);
    const auto maps = perlin2D::generateOctaveMaps(ev.configFor(decl));
    check(maps.size() == 3, "png: 3 oitavas (padrão de NoiseConfig)");
    for (std::size_t i = 0; i < maps.size(); ++i) {
        const auto p = rt::octavePath("oit", "A", static_cast<int>(i));
        check(pngEquals(p, maps[i], 4, 3), "png: " + p + " (D4)");
    }
    check(!fs::exists(rt::octavePath("oit", "A", 3)), "png: nenhuma oitava além da última");

    fs::current_path(oldCwd);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "uso: eval_test <dir de avaliacao/> <dir temporário>\n";
        return 2;
    }
    const std::string dir = fs::absolute(argv[1]).string();
    tabela(dir);
    cache(dir);
    png(dir, argv[2]);

    std::cout << (failures ? std::format("{} falha(s)\n", failures) : "todos os testes passaram\n");
    return failures ? 1 : 0;
}
