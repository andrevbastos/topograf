#pragma once

#include <filesystem>

#include "lang/ast.hpp"
#include "lang/eval.hpp"

namespace display {

// Abre uma janela do ifcg por comando `display`, todas ao mesmo tempo, cada uma em um
// processo próprio (fork), porque o Engine do ifcg é singleton (D15).
// Chamar depois de todos os `exportar` (D10). Bloqueia até todas as janelas fecharem.
// exeDir: diretório do executável (os shaders estão em exeDir/resources, D20).
// frames > 0 fecha cada janela após esse número de frames (testes).
// Retorna 0 se todas as janelas terminaram bem, 1 caso contrário.
int runDisplays(const ast::Program& program, rt::Evaluator& ev,
                const std::filesystem::path& exeDir, int frames);

} // namespace display
