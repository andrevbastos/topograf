%{
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "lang/driver.hpp"
#include "parser.hpp"

// Verdadeiro se o último trecho consumido foi espaço, quebra de linha ou comentário.
// Usado para exigir que a cor venha colada à expressão (D11).
static bool afterSpace = false;

static std::optional<ast::Rgb> parseColor(const std::string& s);

#define YY_USER_ACTION loc.columns(yyleng);
%}

%option noyywrap nounput noinput batch never-interactive

DIGIT    [0-9]
ID       [A-Za-z_][A-Za-z0-9_]*

%%

%{
    yy::location& loc = drv.location;
    loc.step();
%}

[ \t\r]+            { loc.step(); afterSpace = true; }
\n+                 { loc.lines(yyleng); loc.step(); afterSpace = true; }
"//"[^\n]*          { loc.step(); afterSpace = true; }

"terrain"           { afterSpace = false; return yy::parser::make_TERRAIN(loc); }
"perlin"            { afterSpace = false; return yy::parser::make_PERLIN(loc); }
"noise"             { afterSpace = false; return yy::parser::make_NOISE(loc); }
"display"           { afterSpace = false; return yy::parser::make_DISPLAY(loc); }
"export"            { afterSpace = false; return yy::parser::make_EXPORT(loc); }
"as"                { afterSpace = false; return yy::parser::make_AS(loc); }
"to"                { afterSpace = false; return yy::parser::make_TO(loc); }
"octaves"           { afterSpace = false; return yy::parser::make_OCTAVES(loc); }
"depth"             { afterSpace = false; return yy::parser::make_DEPTH(loc); }

"+"                 { afterSpace = false; return yy::parser::make_PLUS(loc); }
"-"                 { afterSpace = false; return yy::parser::make_MINUS(loc); }
"*"                 { afterSpace = false; return yy::parser::make_STAR(loc); }
"/"                 { afterSpace = false; return yy::parser::make_SLASH(loc); }
"("                 { afterSpace = false; return yy::parser::make_LPAREN(loc); }
")"                 { afterSpace = false; return yy::parser::make_RPAREN(loc); }
","                 { afterSpace = false; return yy::parser::make_COMMA(loc); }
";"                 { afterSpace = false; return yy::parser::make_SEMI(loc); }
":"                 { afterSpace = false; return yy::parser::make_COLON(loc); }
"="                 { afterSpace = false; return yy::parser::make_ASSIGN(loc); }

{DIGIT}+("."{DIGIT}+)? {
    afterSpace = false;
    return yy::parser::make_NUMERO(std::strtod(yytext, nullptr), loc);
}

{ID} {
    afterSpace = false;
    return yy::parser::make_IDENT(yytext, loc);
}

\"[^"\n]*\" {
    afterSpace = false;
    return yy::parser::make_STRING(std::string(yytext + 1, yyleng - 2), loc);
}

\"[^"\n]* {
    throw yy::parser::syntax_error(loc, "erro léxico: string sem fechamento");
}

"#"[A-Za-z0-9_]* {
    if (afterSpace) {
        throw yy::parser::syntax_error(loc,
            "erro léxico: espaço antes da cor (a cor deve vir colada à expressão, ex.: A#red)");
    }
    auto color = parseColor(yytext + 1);
    if (!color) {
        throw yy::parser::syntax_error(loc,
            std::string("erro léxico: cor inválida '") + yytext +
            "' (use #RRGGBB ou red, green, blue, white, black, yellow, cyan, magenta, brown, gray)");
    }
    afterSpace = false;
    return yy::parser::make_COR(*color, loc);
}

. {
    throw yy::parser::syntax_error(loc,
        std::string("erro léxico: caractere inválido '") + yytext + "'");
}

<<EOF>>             { return yy::parser::make_END(loc); }

%%

static std::optional<ast::Rgb> parseColor(const std::string& s) {
    struct Named { const char* name; ast::Rgb rgb; };
    static const Named named[] = {
        {"red",     {255,   0,   0}},
        {"green",   {  0, 255,   0}},
        {"blue",    {  0,   0, 255}},
        {"white",   {255, 255, 255}},
        {"black",   {  0,   0,   0}},
        {"yellow",  {255, 255,   0}},
        {"cyan",    {  0, 255, 255}},
        {"magenta", {255,   0, 255}},
        {"brown",   {139,  69,  19}},
        {"gray",    {128, 128, 128}},
    };
    for (const auto& n : named) {
        if (s == n.name) return n.rgb;
    }

    if (s.size() == 6 && s.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
        auto byte = [&](int i) {
            return static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16));
        };
        return ast::Rgb{byte(0), byte(2), byte(4)};
    }
    return std::nullopt;
}

bool Driver::scanBegin() {
    afterSpace = false;
    yyin = std::fopen(file.c_str(), "r");
    if (!yyin) {
        error = "não foi possível abrir '" + file + "': " + std::strerror(errno);
        return false;
    }
    return true;
}

void Driver::scanEnd() {
    std::fclose(yyin);
}
