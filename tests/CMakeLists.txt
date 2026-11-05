# Cada programa .terreno declara o que espera na saída com linhas "// espera: <texto>".
# validos/ deve terminar com código 0; invalidos/ com código 1.
function(add_program_tests dir expected_rc)
    file(GLOB programs "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.terreno")
    foreach(prog ${programs})
        file(RELATIVE_PATH name "${CMAKE_CURRENT_SOURCE_DIR}" "${prog}")
        string(REPLACE ".terreno" "" name "${name}")
        add_test(NAME ${name}
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh" $<TARGET_FILE:perlang> "${prog}" ${expected_rc} ${ARGN})
    endforeach()
endfunction()

add_program_tests(sintaxe/validos 0 --ast)
add_program_tests(sintaxe/invalidos 1 --ast)
add_program_tests(semantica/validos 0 --check)
add_program_tests(semantica/invalidos 1 --check)

# Avaliação e exportação, sem janela (A3)
add_executable(eval_test eval_test.cpp)
target_link_libraries(eval_test PRIVATE perlang_lang)
add_test(NAME avaliacao/eval_test
    COMMAND eval_test "${CMAKE_CURRENT_SOURCE_DIR}/avaliacao" "${CMAKE_CURRENT_BINARY_DIR}/tmp_eval")

# Ponta a ponta: o binário grava os PNGs (--no-display)
file(GLOB export_programs "${CMAKE_CURRENT_SOURCE_DIR}/exportacao/*.terreno")
foreach(prog ${export_programs})
    get_filename_component(name "${prog}" NAME_WE)
    add_test(NAME exportacao/${name}
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_export.sh" $<TARGET_FILE:perlang> "${prog}")
endforeach()
