# Configurações de supressão de avisos para bibliotecas externas
if(TARGET ifcg_core)
    target_compile_options(ifcg_core PRIVATE -Wno-error)
endif()

# Front-end da linguagem: Bison (skeleton C++) + Flex (scanner C)
BISON_TARGET(Parser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp
    COMPILE_FLAGS "-Wall -Werror")
FLEX_TARGET(Lexer lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# Biblioteca da linguagem: análise e avaliação, sem motor gráfico (A3)
add_library(perlang_lang STATIC
    driver.cpp
    ast_dump.cpp
    semantic.cpp
    eval.cpp
    stb_impl.cpp
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)

target_include_directories(perlang_lang PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(perlang main.cpp display.cpp)

target_link_libraries(perlang PRIVATE
    perlang_lang
    ifcg_core
)

# Copia recursos da engine ifcg para o diretório do executável (shaders)
add_custom_command(TARGET perlang POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${ifcg_SOURCE_DIR}/resources $<TARGET_FILE_DIR:perlang>/resources
)
