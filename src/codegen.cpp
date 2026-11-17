#include "lang/codegen.hpp"

#include <format>
#include <sstream>
#include <stdexcept>

namespace codegen {

namespace {

// Literal C++ de string, com escape de \ e ".
std::string str(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        if (ch == '\\' || ch == '"') out += '\\';
        out += ch;
    }
    return out + "\"";
}

// Número da DSL como float, convertido igual ao interpretador (double → float).
std::string num(double v) {
    return std::format("static_cast<float>({})", v);
}

std::string var(const std::string& name) { return "v_" + name; }
std::string cfg(const std::string& name) { return "c_" + name; }

// Expressão no ponto `i` da grade. Espelha rt::Evaluator::evaluate.
std::string expr(const ast::Expr& e) {
    using K = ast::Expr::Kind;
    switch (e.kind) {
        case K::Number:
            return std::format("prt::c01({})", num(e.number));  // campo constante (D9)
        case K::Ref:
            return var(e.name) + "[i]";
        case K::Binary: {
            const auto& lhs = *e.children[0];
            const auto& rhs = *e.children[1];
            switch (e.op) {
                case '+': return std::format("prt::add({}, {})", expr(lhs), expr(rhs));
                case '-': return std::format("prt::sub({}, {})", expr(lhs), expr(rhs));
                case '*': {
                    const bool leftScalar = lhs.kind == K::Number;
                    return std::format("prt::scale({}, {})", num(leftScalar ? lhs.number : rhs.number),
                                       expr(leftScalar ? rhs : lhs));
                }
            }
            throw std::logic_error(std::format("operador '{}' sem geração de código", e.op));
        }
        case K::Call: {
            const auto& a = *e.children[0];
            const auto& b = *e.children[1];
            if (e.name == "mistura") {
                return std::format("prt::mistura({}, {}, {}, {})", expr(a), expr(b),
                                   num(e.children[2]->number), num(e.children[3]->number));
            }
            return std::format("prt::{}({}, {})", e.name, expr(a), expr(b));
        }
    }
    throw std::logic_error("nó sem geração de código");
}

const char* configField(const std::string& param) {
    if (param == "frequencia") return "freq";
    if (param == "oitavas") return "octaves";
    if (param == "semente") return "seed";
    if (param == "persistencia") return "pers";
    if (param == "lacunaridade") return "lac";
    if (param == "amplitude") return "amp";
    if (param == "expoente") return "exp";
    if (param == "onda") return "wave";
    throw std::logic_error("parâmetro sem geração de código: " + param);
}

const char* configCast(const std::string& param) {
    if (param == "oitavas" || param == "semente") return "static_cast<unsigned>";
    if (param == "onda") return "static_cast<int>";
    return "static_cast<float>";
}

} // namespace

std::string generateCpp(const ast::Program& program, const sema::Result& /*sem*/, const std::string& sourceName) {
    int width = 0, length = 0, height = 0;
    for (const auto& p : program.header.params) {
        if (p.name == "width") width = static_cast<int>(p.value);
        else if (p.name == "length") length = static_cast<int>(p.value);
        else if (p.name == "height") height = static_cast<int>(p.value);
    }

    std::ostringstream out;
    out << "// Gerado por topograf a partir de " << str(sourceName) << ". Não editar.\n"
        << "#include <iostream>\n"
        << "#include <string>\n"
        << "#include <vector>\n\n"
        << "#include \"runtime/prt.hpp\"\n\n"
        << "int main(int argc, char** argv) {\n"
        << "    const prt::Options opt = prt::parseOptions(argc, argv);\n"
        << std::format("    const prt::Grid g {{{}, {}, {}}};\n", width, length, height)
        << "    const std::size_t N = g.size();\n"
        << "    int status = 0;\n"
        << "    auto report = [&](const std::string& e) {\n"
        << "        std::cerr << " << str(sourceName + ": ") << " << e << \"\\n\";\n"
        << "        status = 1;\n"
        << "    };\n";

    std::ostringstream displays;
    bool hasDisplay = false;

    for (const auto& cmd : program.commands) {
        std::visit([&](const auto& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, ast::PrimitiveDecl>) {
                out << std::format("\n    // linha {}: ruido {}(...)\n", c.loc.line, c.name)
                    << std::format("    perlin2D::NoiseConfig {} = prt::config(g);\n", cfg(c.name));
                for (const auto& p : c.params) {
                    out << std::format("    {}.{} = {}({});\n", cfg(c.name), configField(p.name), configCast(p.name), p.value);
                }
                out << std::format("    const prt::Field {} = perlin2D::generateNoiseMap({});\n", var(c.name), cfg(c.name));
            } else if constexpr (std::is_same_v<T, ast::CompositeDecl>) {
                out << std::format("\n    // linha {}: ruido {} = {}\n", c.loc.line, c.name, ast::dump(*c.expr))
                    << std::format("    prt::Field {}(N);\n", var(c.name))
                    << std::format("    for (std::size_t i = 0; i < N; ++i) {}[i] = {};\n", var(c.name), expr(*c.expr));
            } else if constexpr (std::is_same_v<T, ast::Export>) {
                out << std::format("\n    // linha {}: exportar {} como {}\n", c.loc.line, c.name, str(c.path))
                    << std::format("    if (auto e = prt::exportPNG(g, {}, {}, {})) report(*e);\n",
                                   var(c.name), str(c.path), c.loc.line);
            } else if constexpr (std::is_same_v<T, ast::ExportOctaves>) {
                out << std::format("\n    // linha {}: exportar oitavas {} em {}\n", c.loc.line, c.name, str(c.dir))
                    << std::format("    for (const auto& e : prt::exportOctaves(g, {}, {}, {}, {})) report(e);\n",
                                   cfg(c.name), str(c.dir), str(c.name), c.loc.line);
            } else if constexpr (std::is_same_v<T, ast::Display>) {
                // D10: displays depois de todos os exportar.
                hasDisplay = true;
                displays << std::format("        // linha {}\n", c.loc.line)
                         << std::format("        displays.push_back({{{}, {{\n", c.loc.line);
                for (const auto& layer : c.layers) {
                    const std::string color = layer.color
                        ? std::format("prt::Rgb {{{}, {}, {}}}", layer.color->r, layer.color->g, layer.color->b)
                        : "std::nullopt";
                    displays << "            {[&] {\n"
                             << "                prt::Field f(N);\n"
                             << std::format("                for (std::size_t i = 0; i < N; ++i) f[i] = {};\n", expr(*layer.expr))
                             << "                return f;\n"
                             << std::format("            }}, {}}},\n", color);
                }
                displays << std::format("        }}, {}}});\n", c.depth ? "true" : "false");
            }
        }, cmd);
    }

    if (hasDisplay) {
        out << "\n    if (!opt.noDisplay) {\n"
            << "        std::vector<prt::DisplaySpec> displays;\n"
            << displays.str()
            << "        if (prt::runDisplays(g, displays, opt.frames) != 0) status = 1;\n"
            << "    }\n";
    } else {
        out << "    (void)opt;\n";
    }
    out << "    return status;\n}\n";
    return out.str();
}

} // namespace codegen
