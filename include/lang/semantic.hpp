#pragma once

#include <map>
#include <string>
#include <vector>

#include "lang/ast.hpp"

namespace sema {

struct Error {
    ast::Loc loc;
    std::string message;
};

// T1: entrada da tabela de símbolos.
struct Symbol {
    enum class Kind { Primitive, Composite };

    std::string name;
    Kind kind;
    int line = 0;
    const ast::PrimitiveDecl* primitive = nullptr;  // parâmetros, se primitivo
    const ast::CompositeDecl* composite = nullptr;  // expressão, se composto
};

struct Result {
    std::map<std::string, Symbol> symbols;
    std::vector<Error> errors;  // acumulados em ordem de ocorrência (D5)

    bool ok() const { return errors.empty(); }
};

Result analyze(const ast::Program& program);

std::string formatError(const Error& e);
std::string dumpSymbols(const Result& result);

} // namespace sema
