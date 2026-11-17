#include <filesystem>
#include <iostream>
#include <string>

#include "lang/ast.hpp"
#include "lang/codegen.hpp"
#include "lang/compile.hpp"
#include "lang/driver.hpp"
#include "lang/eval.hpp"
#include "lang/semantic.hpp"
#include "runtime/prt.hpp"

static int usage() {
	std::cerr << "uso: topograf <programa> [-o saida | --emit-cpp] [--no-display] [--ast] [--check] [--frames N]\n"
	             "  -o saida      compila o programa para o executável `saida` (não executa)\n"
	             "  --emit-cpp    imprime o C++ gerado e encerra\n"
	             "  --ast         imprime a AST e encerra (sem análise semântica)\n"
	             "  --check       análise semântica e tabela de símbolos, sem executar\n"
	             "  --no-display  executa sem abrir janelas\n"
	             "  --frames N    fecha cada janela após N frames (testes)\n"
	             "sem -o nem --emit-cpp, o programa é interpretado.\n";
	return 2;
}

int main(int argc, char** argv) {
	std::string path;
	std::string output;
	bool noDisplay = false;
	bool printAst = false;
	bool checkOnly = false;
	bool emitCpp = false;
	int frames = 0;

	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--no-display") noDisplay = true;
		else if (arg == "--ast") printAst = true;
		else if (arg == "--check") checkOnly = true;
		else if (arg == "--emit-cpp") emitCpp = true;
		else if (arg == "-o" && i + 1 < argc) output = argv[++i];
		else if (arg == "--frames" && i + 1 < argc) {
			try {
				frames = std::stoi(argv[++i]);
			} catch (const std::exception&) {
				return usage();
			}
			if (frames <= 0) return usage();
		}
		else if (arg.starts_with("-") || !path.empty()) return usage();
		else path = arg;
	}
	if (path.empty() || (emitCpp && !output.empty())) return usage();

	Driver driver;
	if (!driver.parse(path)) {
		std::cerr << path << ": " << driver.error << "\n";
		return 1;
	}

	if (printAst) {
		std::cout << ast::dump(driver.program);
		return 0;
	}

	const sema::Result sem = sema::analyze(driver.program);
	if (!sem.ok()) {
		for (const auto& e : sem.errors) std::cerr << path << ": " << sema::formatError(e) << "\n";
		std::cerr << sem.errors.size() << " erro(s) semântico(s)\n";
		return 1;
	}

	if (checkOnly) {
		std::cout << sema::dumpSymbols(sem);
		return 0;
	}

	// Compilador: gera C++ e, com -o, compila para um executável.
	if (emitCpp || !output.empty()) {
		const std::string sourceName = std::filesystem::path(path).filename().string();
		const std::string cpp = codegen::generateCpp(driver.program, sem, sourceName);
		if (emitCpp) {
			std::cout << cpp;
			return 0;
		}
		std::string error;
		if (!compile::buildExecutable(cpp, output, error)) {
			std::cerr << path << ": erro de compilação: " << error << "\n";
			return 1;
		}
		return 0;
	}

	// Interpretador. D10: todos os exportar primeiro, display por último.
	rt::Evaluator ev(driver.program, sem);
	const auto runtimeErrors = rt::runExports(driver.program, ev);
	for (const auto& e : runtimeErrors) std::cerr << path << ": " << e << "\n";

	int displayStatus = 0;
	if (!noDisplay) {
		displayStatus = prt::runDisplays(ev.grid(), rt::displaySpecs(driver.program, ev), frames);
	}

	return runtimeErrors.empty() && displayStatus == 0 ? 0 : 1;
}
