#include "lang/eval.hpp"

#include <format>
#include <stdexcept>

namespace rt {

namespace {

// As operações vêm de prt (runtime): o interpretador e o código gerado pelo
// compilador usam exatamente as mesmas funções.
template <typename F>
Field zip(const Field& a, const Field& b, F op) {
    Field out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = op(a[i], b[i]);
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
    perlin2D::NoiseConfig c = prt::config(grid());
    // Omitidos usam os padrões de NoiseConfig (D7).
    for (const auto& p : decl.params) {
        if (p.name == "frequency") c.freq = static_cast<float>(p.value);
        else if (p.name == "octaves") c.octaves = static_cast<unsigned>(p.value);
        else if (p.name == "seed") c.seed = static_cast<unsigned>(p.value);
        else if (p.name == "persistence") c.pers = static_cast<float>(p.value);
        else if (p.name == "lacunarity") c.lac = static_cast<float>(p.value);
        else if (p.name == "amplitude") c.amp = static_cast<float>(p.value);
        else if (p.name == "exponent") c.exp = static_cast<float>(p.value);
        else if (p.name == "wave") c.wave = static_cast<int>(p.value);
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
            return Field(size(), prt::c01(static_cast<float>(e.number)));

        case K::Ref:
            return field(e.name);

        case K::Binary: {
            const auto& lhs = *e.children[0];
            const auto& rhs = *e.children[1];
            switch (e.op) {
                case '+': return zip(evaluate(lhs), evaluate(rhs), prt::add);
                case '-': return zip(evaluate(lhs), evaluate(rhs), prt::sub);
                case '*': {
                    // k * A: o escalar é o literal sem clamp (T7 garante um lado literal).
                    const bool leftScalar = lhs.kind == K::Number;
                    const float k = static_cast<float>(leftScalar ? lhs.number : rhs.number);
                    Field f = evaluate(leftScalar ? rhs : lhs);
                    for (float& v : f) v = prt::scale(k, v);
                    return f;
                }
            }
            throw std::logic_error(std::format("operador '{}' sem avaliação", e.op));
        }

        case K::Call: {
            const Field a = evaluate(*e.children[0]);
            const Field b = evaluate(*e.children[1]);
            const std::string& fn = e.name;
            if (fn == "max") return zip(a, b, prt::max);
            if (fn == "min") return zip(a, b, prt::min);
            if (fn == "above") return zip(a, b, prt::above);
            if (fn == "below") return zip(a, b, prt::below);
            if (fn == "avg") return zip(a, b, prt::avg);
            if (fn == "mix") {
                const float na = static_cast<float>(e.children[2]->number);
                const float nb = static_cast<float>(e.children[3]->number);
                return zip(a, b, [=](float x, float y) { return prt::mix(x, y, na, nb); });
            }
            throw std::logic_error("função sem avaliação: " + fn);
        }
    }
    throw std::logic_error("nó sem avaliação");
}

std::vector<std::string> runExports(const ast::Program& program, Evaluator& ev) {
    std::vector<std::string> errors;
    const prt::Grid g = ev.grid();

    for (const auto& cmd : program.commands) {
        if (auto* e = std::get_if<ast::Export>(&cmd)) {
            if (auto err = prt::exportPNG(g, ev.field(e->name), e->path, e->loc.line)) errors.push_back(*err);
        } else if (auto* e = std::get_if<ast::ExportOctaves>(&cmd)) {
            for (const auto& c : program.commands) {
                auto* p = std::get_if<ast::PrimitiveDecl>(&c);
                if (!p || p->name != e->name) continue;
                auto errs = prt::exportOctaves(g, ev.configFor(*p), e->dir, e->name, e->loc.line);
                errors.insert(errors.end(), errs.begin(), errs.end());
                break;
            }
        }
    }
    return errors;
}

std::vector<prt::DisplaySpec> displaySpecs(const ast::Program& program, Evaluator& ev) {
    std::vector<prt::DisplaySpec> specs;
    for (const auto& cmd : program.commands) {
        const auto* d = std::get_if<ast::Display>(&cmd);
        if (!d) continue;

        prt::DisplaySpec spec {d->loc.line, {}, d->depth};
        for (const auto& layer : d->layers) {
            std::optional<prt::Rgb> color;
            if (layer.color) color = prt::Rgb {layer.color->r, layer.color->g, layer.color->b};
            const ast::Expr* expr = layer.expr.get();
            spec.layers.push_back({[&ev, expr]() { return ev.evaluate(*expr); }, color});
        }
        specs.push_back(std::move(spec));
    }
    return specs;
}

} // namespace rt
