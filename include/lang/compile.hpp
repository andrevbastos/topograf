#pragma once

#include <string>

namespace compile {

// Compila o C++ gerado com o compilador do sistema e liga com o runtime do perlang
// (perlang_display, perlang_rtcore, ifcg, glfw, GL), gerando o executável `output`.
// Retorna false e preenche `error` se a compilação falhar.
bool buildExecutable(const std::string& cppSource, const std::string& output, std::string& error);

} // namespace compile
