%require "3.8"
%language "c++"
%header

%define api.token.raw
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%define parse.error custom
%locations
%expect 0

%code requires {
    #include <optional>
    #include <string>
    #include <vector>
    #include "lang/ast.hpp"
    class Driver;
}

%param { Driver& drv }

%code {
    #include "lang/driver.hpp"

    static ast::Loc L(const yy::location& l) {
        return {l.begin.line, l.begin.column};
    }
}

%define api.token.prefix {TOK_}
%token
    END 0           "fim do arquivo"
    TERRENO         "terreno"
    RUIDO           "ruido"
    DISPLAY         "display"
    EXPORTAR        "exportar"
    COMO            "como"
    EM              "em"
    OITAVAS         "oitavas"
    PROFUNDIDADE    "profundidade"
    PLUS            "+"
    MINUS           "-"
    STAR            "*"
    SLASH           "/"
    LPAREN          "("
    RPAREN          ")"
    COMMA           ","
    SEMI            ";"
    COLON           ":"
    ASSIGN          "="
;
%token <double>      NUMERO "número"
%token <std::string> IDENT  "identificador"
%token <std::string> STRING "string"
%token <ast::Rgb>    COR    "cor"

%nterm <std::vector<ast::Param>>  params
%nterm <ast::Param>               param
%nterm <std::string>              nome_param
%nterm <ast::ExprPtr>             expr
%nterm <std::vector<ast::ExprPtr>> args
%nterm <std::vector<ast::Layer>>  camadas
%nterm <ast::Layer>               camada
%nterm <std::optional<ast::Rgb>>  opt_cor
%nterm <bool>                     opt_prof

%left "+" "-"
%left "*" "/"

%%

programa
    : cabecalho comandos
    ;

cabecalho
    : "terreno" "(" params ")" ";"
        { drv.program.header = ast::Header{std::move($3), L(@1)}; }
    ;

comandos
    : comandos comando
    | %empty
    ;

comando
    : "ruido" IDENT "(" params ")" ";"
        { drv.program.commands.emplace_back(ast::PrimitiveDecl{std::move($2), std::move($4), L(@1)}); }
    | "ruido" IDENT "=" expr ";"
        { drv.program.commands.emplace_back(ast::CompositeDecl{std::move($2), std::move($4), L(@1)}); }
    | "display" camadas opt_prof ";"
        { drv.program.commands.emplace_back(ast::Display{std::move($2), $3, L(@1)}); }
    | "exportar" IDENT "como" STRING ";"
        { drv.program.commands.emplace_back(ast::Export{std::move($2), std::move($4), L(@1)}); }
    | "exportar" "oitavas" IDENT "em" STRING ";"
        { drv.program.commands.emplace_back(ast::ExportOctaves{std::move($3), std::move($5), L(@1)}); }
    ;

params
    : param                 { $$.push_back(std::move($1)); }
    | params "," param      { $$ = std::move($1); $$.push_back(std::move($3)); }
    ;

param
    : nome_param ":" NUMERO { $$ = ast::Param{std::move($1), $3, L(@1)}; }
    ;

/* `oitavas` é palavra-chave e também nome de parâmetro (D12). */
nome_param
    : IDENT                 { $$ = std::move($1); }
    | "oitavas"             { $$ = "oitavas"; }
    ;

expr
    : expr "+" expr         { $$ = ast::Expr::makeBinary('+', std::move($1), std::move($3), L(@2)); }
    | expr "-" expr         { $$ = ast::Expr::makeBinary('-', std::move($1), std::move($3), L(@2)); }
    | expr "*" expr         { $$ = ast::Expr::makeBinary('*', std::move($1), std::move($3), L(@2)); }
    | expr "/" expr         { $$ = ast::Expr::makeBinary('/', std::move($1), std::move($3), L(@2)); }
    | "(" expr ")"          { $$ = std::move($2); }
    | NUMERO                { $$ = ast::Expr::makeNumber($1, L(@1)); }
    | IDENT                 { $$ = ast::Expr::makeRef(std::move($1), L(@1)); }
    | IDENT "(" args ")"    { $$ = ast::Expr::makeCall(std::move($1), std::move($3), L(@1)); }
    ;

args
    : expr                  { $$.push_back(std::move($1)); }
    | args "," expr         { $$ = std::move($1); $$.push_back(std::move($3)); }
    ;

camadas
    : camada                { $$.push_back(std::move($1)); }
    | camadas "," camada    { $$ = std::move($1); $$.push_back(std::move($3)); }
    ;

camada
    : expr opt_cor          { $$ = ast::Layer{std::move($1), $2}; }
    ;

opt_cor
    : %empty                { $$ = std::nullopt; }
    | COR                   { $$ = $1; }
    ;

/* No máximo um `profundidade` por display (D16). */
opt_prof
    : %empty                { $$ = false; }
    | "profundidade"        { $$ = true; }
    ;

%%

// Mensagem de erro sintático em português, com o token encontrado e os esperados.
void yy::parser::report_syntax_error(const context& ctx) const {
    std::string msg = "erro sintático: ";

    if (ctx.token() == symbol_kind::S_YYEMPTY) {
        msg += "entrada inesperada";
    } else {
        msg += std::string("encontrado ") + symbol_name(ctx.token()) + " inesperado";
    }

    enum { MAX_EXPECTED = 6 };
    symbol_kind_type expected[MAX_EXPECTED];
    const int n = ctx.expected_tokens(expected, MAX_EXPECTED);
    for (int i = 0; i < n; ++i) {
        msg += (i == 0 ? ", esperado " : " ou ");
        msg += symbol_name(expected[i]);
    }

    // report_syntax_error é const; drv é referência, então pode ser alterado.
    if (drv.error.empty()) {
        drv.error = formatError(ctx.location(), msg);
    }
}

// Chamado pelo parser (via report_syntax_error) e para exceções lançadas pelo léxico.
void yy::parser::error(const location_type& loc, const std::string& msg) {
    if (drv.error.empty()) {
        drv.error = formatError(loc, msg);
    }
}
