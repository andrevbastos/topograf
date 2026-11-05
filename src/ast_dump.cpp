#include <format>
#include <sstream>

#include "lang/ast.hpp"

namespace ast {

namespace {

std::string number(double v) {
    return std::format("{}", v);
}

std::string params(const std::vector<Param>& ps) {
    std::string out;
    for (std::size_t i = 0; i < ps.size(); ++i) {
        if (i) out += ", ";
        out += ps[i].name + ": " + number(ps[i].value);
    }
    return out;
}

std::string color(const Rgb& c) {
    return std::format("#{:02x}{:02x}{:02x}", c.r, c.g, c.b);
}

} // namespace

// Prefixada: (op a b), (função a b ...). Literais e nomes aparecem como são.
std::string dump(const Expr& e) {
    switch (e.kind) {
        case Expr::Kind::Number:
            return number(e.number);
        case Expr::Kind::Ref:
            return e.name;
        case Expr::Kind::Binary:
            return std::format("({} {} {})", e.op, dump(*e.children[0]), dump(*e.children[1]));
        case Expr::Kind::Call: {
            std::string out = "(" + e.name;
            for (const auto& a : e.children) out += " " + dump(*a);
            return out + ")";
        }
    }
    return "?";
}

std::string dump(const Program& p) {
    std::ostringstream out;
    out << "terreno(" << params(p.header.params) << ")\n";

    for (const auto& cmd : p.commands) {
        std::visit([&](const auto& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, PrimitiveDecl>) {
                out << "ruido " << c.name << "(" << params(c.params) << ")";
            } else if constexpr (std::is_same_v<T, CompositeDecl>) {
                out << "ruido " << c.name << " = " << dump(*c.expr);
            } else if constexpr (std::is_same_v<T, Display>) {
                out << "display ";
                for (std::size_t i = 0; i < c.layers.size(); ++i) {
                    if (i) out << ", ";
                    out << dump(*c.layers[i].expr);
                    if (c.layers[i].color) out << color(*c.layers[i].color);
                }
                if (c.depth) out << " profundidade";
            } else if constexpr (std::is_same_v<T, Export>) {
                out << "exportar " << c.name << " como \"" << c.path << "\"";
            } else if constexpr (std::is_same_v<T, ExportOctaves>) {
                out << "exportar oitavas " << c.name << " em \"" << c.dir << "\"";
            }
            out << "  @" << c.loc.line << "\n";
        }, cmd);
    }
    return out.str();
}

} // namespace ast
