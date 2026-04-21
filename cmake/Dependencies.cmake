include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

set(CLI11_BUILD_DOCS OFF CACHE INTERNAL "")
set(CLI11_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(CLI11_BUILD_TESTS OFF CACHE INTERNAL "")
set(CLI11_BUILD_TOOLS OFF CACHE INTERNAL "")
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.1
)

set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")
set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.2
)

FetchContent_MakeAvailable(nlohmann_json cli11 doctest)
