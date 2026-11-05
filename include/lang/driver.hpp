#pragma once

#include <string>

#include "lang/ast.hpp"
#include "parser.hpp"

class Driver;

#define YY_DECL yy::parser::symbol_type yylex(Driver& drv)
YY_DECL;

// Conduz léxico + parser sobre um arquivo. Erros léxicos e sintáticos
// interrompem a análise no primeiro erro (D17).
class Driver {
public:
    ast::Program program;
    yy::location location;
    std::string file;
    std::string error;  // vazio se a análise foi bem-sucedida

    bool parse(const std::string& path);

    // Implementados em lexer.l (precisam de yyin).
    bool scanBegin();
    void scanEnd();
};

std::string formatError(const yy::location& loc, const std::string& msg);
