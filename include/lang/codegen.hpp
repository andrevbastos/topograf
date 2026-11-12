#pragma once

#include <string>

#include "lang/ast.hpp"
#include "lang/semantic.hpp"

namespace codegen {

// Traduz um programa sem erros semânticos para C++ que usa o runtime (runtime/prt.hpp).
// Cada primitivo vira uma chamada ao gerador Perlin; cada composto vira um único laço
// sobre a grade com a expressão inteira embutida; cada display vira uma lista de
// camadas avaliadas no processo da janela. `sourceName` aparece nas mensagens de erro.
std::string generateCpp(const ast::Program& program, const sema::Result& sem, const std::string& sourceName);

} // namespace codegen
