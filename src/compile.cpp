#include "lang/compile.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

#include "build_config.hpp"  // gerado pelo CMake: compilador, includes e bibliotecas

namespace fs = std::filesystem;

namespace compile {

namespace {

// Executa `args` (sem shell) e retorna o código de saída, ou -1 se não conseguiu executar.
int run(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int ws = 0;
    while (waitpid(pid, &ws, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(ws) ? WEXITSTATUS(ws) : -1;
}

} // namespace

bool buildExecutable(const std::string& cppSource, const std::string& output, std::string& error) {
    const fs::path cppFile = fs::temp_directory_path() / std::format("topograf-{}.cpp", getpid());
    {
        std::ofstream f(cppFile);
        f << cppSource;
        if (!f) {
            error = "não foi possível gravar o C++ gerado em " + cppFile.string();
            return false;
        }
    }

    // CXX no ambiente substitui o compilador usado no build do topograf.
    const char* envCxx = std::getenv("CXX");
    std::vector<std::string> args {
        envCxx && *envCxx ? envCxx : TOPOGRAF_CXX,
        "-std=c++20", "-O2",
        "-ffp-contract=off",  // sem FMA: mesmo resultado em float que o interpretador
        "-w",                 // avisos de cabeçalhos de terceiros não interessam ao usuário
        "-I", TOPOGRAF_INCLUDE_DIR,
        cppFile.string(),
        "-o", output,
    };
    for (const char* lib : TOPOGRAF_LINK_LIBS) args.emplace_back(lib);

    const int rc = run(args);
    fs::remove(cppFile);

    if (rc != 0) {
        error = rc == 127 || rc == -1
            ? std::format("não foi possível executar o compilador '{}'", args[0])
            : std::format("o compilador '{}' falhou (código {}); use --emit-cpp para ver o C++ gerado", args[0], rc);
        return false;
    }
    return true;
}

} // namespace compile
