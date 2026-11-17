# Configurações de supressão de avisos para bibliotecas externas
if(TARGET ifcg_core)
    target_compile_options(ifcg_core PRIVATE -Wno-error)
    # Os executáveis gerados pelo compilador são PIE e ligam com esta biblioteca.
    set_target_properties(ifcg_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ---------------------------------------------------------------------------
# Runtime: usado pelo interpretador e pelos executáveis gerados (-o)
# ---------------------------------------------------------------------------

# Núcleo sem motor gráfico: operações, Perlin, PNG, opções de linha de comando
add_library(topograf_rtcore STATIC
    runtime/prt.cpp
    stb_impl.cpp
)
target_include_directories(topograf_rtcore PUBLIC "${PROJECT_SOURCE_DIR}/include")
set_target_properties(topograf_rtcore PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Display com ifcg (D1, D15, D19, D20)
add_library(topograf_display STATIC runtime/display.cpp)
target_link_libraries(topograf_display PUBLIC topograf_rtcore ifcg_core)
# Shaders do ifcg para executáveis gerados fora do diretório do build (D20)
target_compile_definitions(topograf_display PRIVATE
    TOPOGRAF_RESOURCES_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources")
set_target_properties(topograf_display PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Front-end e back-end da linguagem
# ---------------------------------------------------------------------------

# Bison (skeleton C++) + Flex (scanner C)
BISON_TARGET(Parser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp
    COMPILE_FLAGS "-Wall -Werror")
FLEX_TARGET(Lexer lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# Análise, avaliação e geração de código, sem motor gráfico (A3)
add_library(topograf_lang STATIC
    driver.cpp
    ast_dump.cpp
    semantic.cpp
    eval.cpp
    codegen.cpp
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)
target_include_directories(topograf_lang PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(topograf_lang PUBLIC topograf_rtcore)

# Caminhos usados pelo compilador ao gerar executáveis (-o)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build_config.hpp" CONTENT
"#pragma once
// Gerado pelo CMake. Caminhos usados por `topograf -o` para compilar o C++ gerado.
#define TOPOGRAF_CXX \"${CMAKE_CXX_COMPILER}\"
#define TOPOGRAF_INCLUDE_DIR \"${PROJECT_SOURCE_DIR}/include\"
static const char* const TOPOGRAF_LINK_LIBS[] = {
    \"$<TARGET_FILE:topograf_display>\",
    \"$<TARGET_FILE:topograf_rtcore>\",
    \"$<TARGET_FILE:ifcg_core>\",
    \"-lglfw\", \"-lGL\", \"-pthread\",
};
")

add_executable(topograf main.cpp compile.cpp)
target_link_libraries(topograf PRIVATE topograf_lang topograf_display)

# Copia recursos da engine ifcg para o diretório do executável (shaders)
add_custom_command(TARGET topograf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${ifcg_SOURCE_DIR}/resources $<TARGET_FILE_DIR:topograf>/resources
)
