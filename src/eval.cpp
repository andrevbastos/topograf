#include "lang/eval.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <stdexcept>

namespace rt {

namespace {

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

template <typename F>
Field zip(const Field& a, const Field& b, F op) {
    Field out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = clamp01(op(a[i], b[i]));
    return out;
}

} // namespace

Evaluator::Evaluator(const ast::Program& program, const sema::Result& sem)
    : program_(program), sem_(sem) {
    for (const auto& p : program.header.params) {
        if (p.name == "width") width_ = static_cast<int>(p.value);
        else if (p.name == "length") length_ = static_cast<int>(p.value);
        else if (p.name == "height") height_ = static_cast<int>(p.value);
    }
}

perlin2D::NoiseConfig Evaluator::configFor(const ast::PrimitiveDecl& decl) const {
    perlin2D::NoiseConfig c {};
    c.width = width_;
    c.height = length_;  // NoiseConfig.height = length da DSL (1.1.3)
    // Omitidos usam os padrões de NoiseConfig (D7).
    for (const auto& p : decl.params) {
        if (p.name == "frequencia") c.freq = static_cast<float>(p.value);
        else if (p.name == "oitavas") c.octaves = static_cast<unsigned>(p.value);
        else if (p.name == "semente") c.seed = static_cast<unsigned>(p.value);
        else if (p.name == "persistencia") c.pers = static_cast<float>(p.value);
        else if (p.name == "lacunaridade") c.lac = static_cast<float>(p.value);
        else if (p.name == "amplitude") c.amp = static_cast<float>(p.value);
        else if (p.name == "expoente") c.exp = static_cast<float>(p.value);
        else if (p.name == "onda") c.wave = static_cast<int>(p.value);
    }
    return c;
}

void Evaluator::preload(const std::string& name, Field values) {
    if (values.size() != size()) throw std::invalid_argument("preload: tamanho diferente da grade");
    cache_[name] = std::move(values);
}

const Field& Evaluator::field(const std::string& name) {
    if (auto it = cache_.find(name); it != cache_.end()) return it->second;

    const auto& sym = sem_.symbols.at(name);
    Field f;
    if (sym.kind == sema::Symbol::Kind::Primitive) {
        f = perlin2D::generateNoiseMap(configFor(*sym.primitive));
        ++generations_;
    } else {
        f = evaluate(*sym.composite->expr);
    }
    return cache_.emplace(name, std::move(f)).first->second;
}

Field Evaluator::evaluate(const ast::Expr& e) {
    using K = ast::Expr::Kind;
    switch (e.kind) {
        case K::Number:
            // Número em posição de ruído vira campo constante (D9).
            return Field(size(), clamp01(static_cast<float>(e.number)));

        case K::Ref:
            return field(e.name);

        case K::Binary: {
            const auto& lhs = *e.children[0];
            const auto& rhs = *e.children[1];
            switch (e.op) {
                case '+': return zip(evaluate(lhs), evaluate(rhs), [](float a, float b) { return a + b; });
                case '-': return zip(evaluate(lhs), evaluate(rhs), [](float a, float b) { return a - b; });
                case '*': {
                    // k * A: o escalar é o literal sem clamp (T7 garante um lado literal).
                    const bool leftScalar = lhs.kind == K::Number;
                    const float k = static_cast<float>(leftScalar ? lhs.number : rhs.number);
                    Field f = evaluate(leftScalar ? rhs : lhs);
                    for (float& v : f) v = clamp01(k * v);
                    return f;
                }
            }
            throw std::logic_error(std::format("operador '{}' sem avaliação", e.op));
        }

        case K::Call: {
            const Field a = evaluate(*e.children[0]);
            const Field b = evaluate(*e.children[1]);
            const std::string& fn = e.name;
            if (fn == "max") return zip(a, b, [](float x, float y) { return std::max(x, y); });
            if (fn == "min") return zip(a, b, [](float x, float y) { return std::min(x, y); });
            if (fn == "acima") return zip(a, b, [](float x, float y) { return x > y ? x : 0.0f; });
            if (fn == "abaixo") return zip(a, b, [](float x, float y) { return x < y ? x : 0.0f; });
            if (fn == "media") return zip(a, b, [](float x, float y) { return (x + y) / 2.0f; });
            if (fn == "mistura") {
                const float na = static_cast<float>(e.children[2]->number);
                const float nb = static_cast<float>(e.children[3]->number);
                return zip(a, b, [=](float x, float y) { return (x * na + y * nb) / (na + nb); });
            }
            throw std::logic_error("função sem avaliação: " + fn);
        }
    }
    throw std::logic_error("nó sem avaliação");
}

std::string octavePath(const std::string& dir, const std::string& name, int i) {
    return (std::filesystem::path(dir) / std::format("{}_oitava_{}.png", name, i)).string();
}

std::vector<std::string> runExports(const ast::Program& program, Evaluator& ev) {
    std::vector<std::string> errors;

    auto save = [&](const std::string& path, const Field& f, ast::Loc loc) {
        try {
            if (!perlin2D::saveNoiseAsPNG(path, f, ev.width(), ev.length())) {
                errors.push_back(std::format("linha {}: erro de execução: não foi possível gravar '{}'", loc.line, path));
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            errors.push_back(std::format("linha {}: erro de execução: {}", loc.line, ex.what()));
        }
    };

    for (const auto& cmd : program.commands) {
        if (auto* e = std::get_if<ast::Export>(&cmd)) {
            save(e->path, ev.field(e->name), e->loc);
        } else if (auto* e = std::get_if<ast::ExportOctaves>(&cmd)) {
            for (const auto& c : program.commands) {
                auto* p = std::get_if<ast::PrimitiveDecl>(&c);
                if (!p || p->name != e->name) continue;
                const auto maps = perlin2D::generateOctaveMaps(ev.configFor(*p));
                for (std::size_t i = 0; i < maps.size(); ++i) {
                    save(octavePath(e->dir, e->name, static_cast<int>(i)), maps[i], e->loc);
                }
                break;
            }
        }
    }
    return errors;
}

} // namespace rt
