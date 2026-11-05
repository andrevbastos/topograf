#include "lang/semantic.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <format>
#include <set>
#include <sstream>
#include <string_view>

namespace sema {

namespace {

struct Builtin {
    std::string_view name;
    std::size_t arity;
};

constexpr std::array<Builtin, 6> builtins {{
    {"max", 2}, {"min", 2}, {"acima", 2}, {"abaixo", 2}, {"media", 2}, {"mistura", 4},
}};

const Builtin* findBuiltin(const std::string& name) {
    for (const auto& b : builtins) {
        if (b.name == name) return &b;
    }
    return nullptr;
}

std::string num(double v) { return std::format("{}", v); }

bool isLiteral(const ast::Expr& e) { return e.kind == ast::Expr::Kind::Number; }

class Analyzer {
public:
    explicit Analyzer(const ast::Program& p) : program(p) {}

    Result run() {
        collectDeclarationLines();
        checkHeader();
        for (const auto& cmd : program.commands) {
            std::visit([&](const auto& c) { check(c); }, cmd);
        }
        return std::move(result);
    }

private:
    const ast::Program& program;
    Result result;
    std::map<std::string, int> firstDeclLine;  // para "usado antes de declarado"
    bool hasHeight = false;

    void error(ast::Loc loc, std::string msg) {
        result.errors.push_back({loc, std::move(msg)});
    }

    void collectDeclarationLines() {
        for (const auto& cmd : program.commands) {
            if (auto* p = std::get_if<ast::PrimitiveDecl>(&cmd)) firstDeclLine.try_emplace(p->name, p->loc.line);
            if (auto* c = std::get_if<ast::CompositeDecl>(&cmd)) firstDeclLine.try_emplace(c->name, c->loc.line);
        }
    }

    // ---------- parâmetros ----------

    // Inteiro sem parte fracionária e representável em [0, max].
    bool checkInteger(const ast::Param& p, double max) {
        if (p.value != std::floor(p.value)) {
            error(p.loc, std::format("'{}' deve ser inteiro (recebeu {})", p.name, num(p.value)));
            return false;
        }
        if (p.value > max) {
            error(p.loc, std::format("'{}' grande demais (máximo {})", p.name, num(max)));
            return false;
        }
        return true;
    }

    void checkPositive(const ast::Param& p) {
        if (p.value <= 0.0) error(p.loc, std::format("'{}' deve ser > 0 (recebeu {})", p.name, num(p.value)));
    }

    // Detecta parâmetros repetidos (T3). Retorna os nomes vistos.
    std::set<std::string> checkDuplicates(const std::vector<ast::Param>& params) {
        std::map<std::string, ast::Loc> seen;
        std::set<std::string> names;
        for (const auto& p : params) {
            auto [it, inserted] = seen.try_emplace(p.name, p.loc);
            if (!inserted) {
                error(p.loc, std::format("parâmetro '{}' repetido (já definido na coluna {})",
                                         p.name, it->second.column));
            }
            names.insert(p.name);
        }
        return names;
    }

    // T4
    void checkHeader() {
        const auto& h = program.header;
        const auto names = checkDuplicates(h.params);

        for (const auto& p : h.params) {
            if (p.name == "width" || p.name == "length" || p.name == "height") {
                if (checkInteger(p, INT_MAX)) checkPositive(p);
            } else {
                error(p.loc, std::format("parâmetro desconhecido '{}' em terreno (aceitos: width, length, height)", p.name));
            }
        }
        for (const char* required : {"width", "length"}) {
            if (!names.contains(required)) error(h.loc, std::format("terreno sem '{}' (obrigatório)", required));
        }
        hasHeight = names.contains("height");
    }

    // ---------- declarações ----------

    // T2/T3: nome livre? Registra o símbolo se for.
    void declare(const std::string& name, ast::Loc loc, Symbol sym) {
        if (findBuiltin(name)) {
            error(loc, std::format("'{}' é nome de função embutida e não pode ser declarado", name));
            return;
        }
        if (auto it = result.symbols.find(name); it != result.symbols.end()) {
            error(loc, std::format("'{}' já declarado na linha {}", name, it->second.line));
            return;
        }
        result.symbols.emplace(name, std::move(sym));
    }

    // T5
    void check(const ast::PrimitiveDecl& d) {
        checkDuplicates(d.params);
        for (const auto& p : d.params) {
            if (p.name == "frequencia" || p.name == "persistencia" || p.name == "lacunaridade" ||
                p.name == "amplitude" || p.name == "expoente") {
                checkPositive(p);
            } else if (p.name == "onda") {
                if (checkInteger(p, INT_MAX)) checkPositive(p);
            } else if (p.name == "oitavas") {
                if (checkInteger(p, UINT_MAX) && p.value < 1) {
                    error(p.loc, std::format("'oitavas' deve ser >= 1 (recebeu {})", num(p.value)));
                }
            } else if (p.name == "semente") {
                checkInteger(p, UINT_MAX);  // NUMERO não tem sinal: >= 0 garantido
            } else if (p.name == "width" || p.name == "length" || p.name == "height") {
                error(p.loc, std::format("'{}' não pode ser definido em ruido; as dimensões vêm de terreno", p.name));
            } else {
                error(p.loc, std::format("parâmetro desconhecido '{}' em ruido {} "
                                         "(aceitos: frequencia, oitavas, semente, persistencia, lacunaridade, amplitude, expoente, onda)", p.name, d.name));
            }
        }
        declare(d.name, d.loc, {d.name, Symbol::Kind::Primitive, d.loc.line, &d, nullptr});
    }

    void check(const ast::CompositeDecl& d) {
        // A expressão é analisada antes de declarar: `ruido C = C + A` é uso antes da declaração.
        checkExpr(*d.expr);
        declare(d.name, d.loc, {d.name, Symbol::Kind::Composite, d.loc.line, nullptr, &d});
    }

    void check(const ast::Display& d) {
        if (!hasHeight) error(d.loc, "display exige 'height' em terreno");  // T11
        for (const auto& layer : d.layers) checkExpr(*layer.expr);
    }

    void check(const ast::Export& e) {
        lookup(e.name, e.loc);
    }

    // T10
    void check(const ast::ExportOctaves& e) {
        const Symbol* s = lookup(e.name, e.loc);
        if (s && s->kind != Symbol::Kind::Primitive) {
            error(e.loc, std::format("'exportar oitavas' exige um ruído primitivo; '{}' é composto", e.name));
        }
    }

    // ---------- expressões ----------

    const Symbol* lookup(const std::string& name, ast::Loc loc) {
        if (auto it = result.symbols.find(name); it != result.symbols.end()) return &it->second;

        if (findBuiltin(name)) {
            error(loc, std::format("'{}' é função embutida e precisa de argumentos", name));
        } else if (auto it = firstDeclLine.find(name); it != firstDeclLine.end() && it->second >= loc.line) {
            error(loc, std::format("'{}' usado antes de ser declarado (declarado na linha {})", name, it->second));
        } else {
            error(loc, std::format("'{}' não declarado", name));
        }
        return nullptr;
    }

    // Retorna true se a expressão é um campo (ruído), false se é constante numérica.
    bool checkExpr(const ast::Expr& e) {
        using K = ast::Expr::Kind;
        switch (e.kind) {
            case K::Number:
                return false;

            case K::Ref:
                lookup(e.name, e.loc);
                return true;

            case K::Binary: {
                const auto& lhs = *e.children[0];
                const auto& rhs = *e.children[1];
                const bool lf = checkExpr(lhs);
                const bool rf = checkExpr(rhs);

                if (e.op == '/') {
                    error(e.loc, "divisão não suportada");  // T8
                } else if (e.op == '*' && !isLiteral(lhs) && !isLiteral(rhs)) {  // T7
                    error(e.loc, "'*' exige que ao menos um lado seja número literal (ruído × ruído não é suportado)");
                }
                return lf || rf;
            }

            case K::Call:
                checkCall(e);
                return true;
        }
        return true;
    }

    // T6, T9
    void checkCall(const ast::Expr& e) {
        for (const auto& arg : e.children) checkExpr(*arg);

        const Builtin* b = findBuiltin(e.name);
        if (!b) {
            if (result.symbols.contains(e.name)) {
                error(e.loc, std::format("'{}' é um ruído, não uma função", e.name));
            } else {
                error(e.loc, std::format("função desconhecida '{}' (disponíveis: max, min, acima, abaixo, media, mistura)", e.name));
            }
            return;
        }
        if (e.children.size() != b->arity) {
            error(e.loc, std::format("'{}' espera {} argumentos, recebeu {}", e.name, b->arity, e.children.size()));
            return;
        }
        if (e.name == "mistura") {
            bool literals = true;
            for (std::size_t i = 2; i < 4; ++i) {
                if (!isLiteral(*e.children[i])) {
                    error(e.children[i]->loc, std::format("argumento {} de 'mistura' deve ser número literal", i + 1));
                    literals = false;
                }
            }
            if (literals && e.children[2]->number + e.children[3]->number == 0.0) {
                error(e.loc, "'mistura' com pesos 0 e 0 (a soma dos pesos deve ser > 0)");
            }
        }
    }
};

} // namespace

Result analyze(const ast::Program& program) {
    return Analyzer(program).run();
}

std::string formatError(const Error& e) {
    return std::format("linha {}, coluna {}: erro semântico: {}", e.loc.line, e.loc.column, e.message);
}

std::string dumpSymbols(const Result& result) {
    // Ordem de declaração, não alfabética.
    std::vector<const Symbol*> ordered;
    for (const auto& [_, s] : result.symbols) ordered.push_back(&s);
    std::sort(ordered.begin(), ordered.end(), [](auto* a, auto* b) { return a->line < b->line; });

    std::ostringstream out;
    for (const Symbol* s : ordered) {
        out << s->name << " " << (s->kind == Symbol::Kind::Primitive ? "primitivo" : "composto")
            << " linha " << s->line;
        if (s->primitive) {
            out << " (";
            for (std::size_t i = 0; i < s->primitive->params.size(); ++i) {
                const auto& p = s->primitive->params[i];
                out << (i ? ", " : "") << p.name << ": " << num(p.value);
            }
            out << ")";
        }
        out << "\n";
    }
    return out.str();
}

} // namespace sema
