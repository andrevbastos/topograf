#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ast {

struct Loc {
    int line = 0;
    int column = 0;
};

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
};

struct Param {
    std::string name;
    double value = 0.0;
    Loc loc;
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

// Parênteses não geram nó: `(2)` é o próprio literal (D14).
struct Expr {
    enum class Kind { Number, Ref, Binary, Call };

    Kind kind;
    Loc loc;
    double number = 0.0;            // Number
    std::string name;               // Ref, Call
    char op = 0;                    // Binary: + - * /
    std::vector<ExprPtr> children;  // Binary: {lhs, rhs}; Call: argumentos

    static ExprPtr makeNumber(double v, Loc l) {
        auto e = std::make_unique<Expr>(Kind::Number, l);
        e->number = v;
        return e;
    }

    static ExprPtr makeRef(std::string n, Loc l) {
        auto e = std::make_unique<Expr>(Kind::Ref, l);
        e->name = std::move(n);
        return e;
    }

    static ExprPtr makeBinary(char o, ExprPtr lhs, ExprPtr rhs, Loc l) {
        auto e = std::make_unique<Expr>(Kind::Binary, l);
        e->op = o;
        e->children.push_back(std::move(lhs));
        e->children.push_back(std::move(rhs));
        return e;
    }

    static ExprPtr makeCall(std::string n, std::vector<ExprPtr> args, Loc l) {
        auto e = std::make_unique<Expr>(Kind::Call, l);
        e->name = std::move(n);
        e->children = std::move(args);
        return e;
    }
};

struct Header {
    std::vector<Param> params;
    Loc loc;
};

// ruido X(params);
struct PrimitiveDecl {
    std::string name;
    std::vector<Param> params;
    Loc loc;
};

// ruido X = expr;
struct CompositeDecl {
    std::string name;
    ExprPtr expr;
    Loc loc;
};

struct Layer {
    ExprPtr expr;
    std::optional<Rgb> color;
};

struct Display {
    std::vector<Layer> layers;
    bool depth = false;
    Loc loc;
};

// exportar X como "f.png";
struct Export {
    std::string name;
    std::string path;
    Loc loc;
};

// exportar oitavas X em "dir/";
struct ExportOctaves {
    std::string name;
    std::string dir;
    Loc loc;
};

using Command = std::variant<PrimitiveDecl, CompositeDecl, Display, Export, ExportOctaves>;

struct Program {
    Header header;
    std::vector<Command> commands;
};

// Representação textual da AST, usada por `--ast` e pelos testes.
std::string dump(const Program& program);
std::string dump(const Expr& expr);

} // namespace ast
