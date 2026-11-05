include(FetchContent)

# --- Bibliotecas pessoais ---

find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)

FetchContent_Declare(
  ifcg
  GIT_REPOSITORY https://github.com/andrevbastos/ifcg.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(ifcg)

# --- Ferramentas do front-end da linguagem ---

find_package(BISON 3.8 REQUIRED)
find_package(FLEX 2.6 REQUIRED)
