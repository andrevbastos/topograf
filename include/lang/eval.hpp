#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/noise_gen.hpp"
#include "lang/ast.hpp"
#include "lang/semantic.hpp"

namespace rt {

// Grade width × length em ordem de linha (índice = j * width + i), valores em [0, 1].
using Field = std::vector<float>;

// Avalia ruídos sobre a grade do cabeçalho. Não depende do motor gráfico (A3).
// Pressupõe um programa sem erros semânticos.
class Evaluator {
public:
    Evaluator(const ast::Program& program, const sema::Result& sem);

    int width() const { return width_; }
    int length() const { return length_; }
    std::optional<int> height() const { return height_; }

    // Campo de um ruído declarado. Primitivos e compostos são calculados uma vez
    // e guardados em cache por nome (A1).
    const Field& field(const std::string& name);

    // Avalia uma expressão (seção 4 do handoff), com clamp em [0, 1] (A2).
    Field evaluate(const ast::Expr& e);

    // Parâmetros do primitivo convertidos para o gerador (D7).
    perlin2D::NoiseConfig configFor(const ast::PrimitiveDecl& decl) const;

    // Substitui o campo de um ruído (testes da tabela de 2 pontos).
    void preload(const std::string& name, Field values);

    std::size_t primitiveGenerations() const { return generations_; }

private:
    const ast::Program& program_;
    const sema::Result& sem_;
    int width_ = 0;
    int length_ = 0;
    std::optional<int> height_;
    std::map<std::string, Field> cache_;
    std::size_t generations_ = 0;

    std::size_t size() const { return static_cast<std::size_t>(width_) * static_cast<std::size_t>(length_); }
};

// Executa todos os `exportar` na ordem do programa (D10).
// Retorna as mensagens de erro de execução (vazio se tudo deu certo).
std::vector<std::string> runExports(const ast::Program& program, Evaluator& ev);

// Caminho do PNG da oitava i de `name` em `dir` (D4).
std::string octavePath(const std::string& dir, const std::string& name, int i);

} // namespace rt
