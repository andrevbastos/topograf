#include "lang/driver.hpp"

bool Driver::parse(const std::string& path) {
    file = path;
    location.initialize(&file);
    error.clear();

    if (!scanBegin()) return false;
    yy::parser parser(*this);
    const int rc = parser.parse();
    scanEnd();

    return rc == 0 && error.empty();
}

std::string formatError(const yy::location& loc, const std::string& msg) {
    return "linha " + std::to_string(loc.begin.line) +
           ", coluna " + std::to_string(loc.begin.column) + ": " + msg;
}
