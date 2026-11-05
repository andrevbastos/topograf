#include <filesystem>
#include <iostream>
#include <string>

#include "lang/ast.hpp"
#include "lang/display.hpp"
#include "lang/driver.hpp"
#include "lang/eval.hpp"
#include "lang/semantic.hpp"

static int usage() {
	std::cerr << "uso: perlang <programa> [--no-display] [--ast] [--check] [--frames N]\n"
	             "  --ast         imprime a AST e encerra (sem análise semântica)\n"
	             "  --check       análise semântica e tabela de símbolos, sem executar\n"
	             "  --no-display  executa sem abrir janelas\n"
	             "  --frames N    fecha cada janela após N frames (testes)\n";
	return 2;
}

int main(int argc, char** argv) {
	std::string path;
	bool noDisplay = false;
	bool printAst = false;
	bool checkOnly = false;
	int frames = 0;

	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--no-display") noDisplay = true;
		else if (arg == "--ast") printAst = true;
		else if (arg == "--check") checkOnly = true;
		else if (arg == "--frames" && i + 1 < argc) {
			try {
				frames = std::stoi(argv[++i]);
			} catch (const std::exception&) {
				return usage();
			}
			if (frames <= 0) return usage();
		}
		else if (arg.starts_with("--") || !path.empty()) return usage();
		else path = arg;
	}
	if (path.empty()) return usage();

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

	// D10: todos os exportar primeiro, display por último (a janela bloqueia).
	rt::Evaluator ev(driver.program, sem);
	const auto runtimeErrors = rt::runExports(driver.program, ev);
	for (const auto& e : runtimeErrors) std::cerr << path << ": " << e << "\n";

	int displayStatus = 0;
	if (!noDisplay) {
		std::error_code ec;
		const auto exeDir = std::filesystem::canonical("/proc/self/exe", ec).parent_path();
		if (ec) {
			std::cerr << path << ": erro de execução: não foi possível localizar o executável\n";
			return 1;
		}
		displayStatus = display::runDisplays(driver.program, ev, exeDir, frames);
	}

	return runtimeErrors.empty() && displayStatus == 0 ? 0 : 1;
}
