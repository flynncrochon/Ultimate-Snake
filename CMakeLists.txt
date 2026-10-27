cmake_minimum_required(VERSION 3.20)
project(SnakeSurvivors VERSION 0.1.0 LANGUAGES C CXX)

option(SNAKE_BUILD_GAME "Build the raylib game (desktop or Emscripten web)" ON)
option(SNAKE_RUN_STRESS_TESTS "Include the full-run simulation stress test in CTest" OFF)
option(SNAKE_RUN_NATIVE_TESTS "Include tests requiring a desktop OpenGL context in CTest" OFF)
option(SNAKE_RUN_BALANCE_TESTS "Include seeded base-versus-evolution combat checks in CTest" OFF)
set(SNAKE_PYTHON_EXECUTABLE "" CACHE FILEPATH "Optional Python 3.10+ interpreter for CTest game tests")
if(EMSCRIPTEN)
    # Native test hosts and CTest's Python subprocess protocol cannot run in a browser.
    set(BUILD_TESTING OFF CACHE BOOL "Build native regression tests" FORCE)
    set(PLATFORM Web CACHE STRING "raylib platform" FORCE)
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "raylib graphics API" FORCE)
endif()
include(CTest)

add_library(snake_core STATIC src/core.cpp src/enemy_system.cpp src/grey_snake_system.cpp src/nest_system.cpp src/weapon_spec.cpp src/weapon_system.cpp src/snake_geometry.cpp src/audio_design.cpp src/render_motion.cpp src/settings.cpp)
target_include_directories(snake_core PUBLIC src)
target_compile_features(snake_core PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(snake_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(snake_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NOT EMSCRIPTEN)
    add_library(snake_test_api STATIC src/game_test_api.cpp)
    target_link_libraries(snake_test_api PUBLIC snake_core)
    add_executable(snake_game_test src/game_test_main.cpp)
    target_link_libraries(snake_game_test PRIVATE snake_test_api)
    set_target_properties(snake_game_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
endif()
add_custom_target(snake_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data" "${CMAKE_BINARY_DIR}/bin/data"
    COMMENT "Synchronizing editable game definitions and assets"
)
if(NOT EMSCRIPTEN)
    add_dependencies(snake_game_test snake_assets)
endif()

if(SNAKE_BUILD_GAME)
    # A host-installed raylib cannot be linked into a WebAssembly module.
    if(NOT EMSCRIPTEN)
        find_package(raylib 5.5 QUIET)
    endif()
    if(NOT raylib_FOUND)
        include(FetchContent)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        # Set FETCHCONTENT_SOURCE_DIR_RAYLIB to a local checkout for offline builds.
        FetchContent_Declare(raylib
            URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
            URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(raylib)
    endif()

    set(snake_display_source src/display_settings.cpp)
    if(EMSCRIPTEN)
        set(snake_display_source src/display_settings_web.cpp)
    endif()
    add_library(snake_presentation STATIC src/presentation.cpp src/world_renderer.cpp src/pixel_art.cpp src/effect_art.cpp src/death_art.cpp src/world_raster.cpp ${snake_display_source} src/settings_presentation.cpp)
    target_compile_features(snake_presentation PUBLIC cxx_std_17)
    target_link_libraries(snake_presentation PUBLIC snake_core raylib)
    if(NOT EMSCRIPTEN)
        if(NOT TARGET glfw)
            find_package(glfw3 3.3 REQUIRED)
        endif()
        target_link_libraries(snake_presentation PRIVATE glfw)
    endif()
    add_executable(snake_survivors src/main.cpp)
    if(WIN32 AND NOT EMSCRIPTEN)
        # Launch without a console, preserving main() and redirected test API I/O.
        set_target_properties(snake_survivors PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(snake_survivors PRIVATE /ENTRY:mainCRTStartup)
        endif()
        enable_language(RC)
        configure_file(src/windows_icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/snake_icon.rc" @ONLY)
        target_sources(snake_survivors PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/snake_icon.rc")
        set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/snake_icon.rc" PROPERTIES
            OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/ultimate-snake.ico")
    endif()
    target_compile_features(snake_survivors PRIVATE cxx_std_17)
    target_link_libraries(snake_survivors PRIVATE snake_presentation)
    if(EMSCRIPTEN)
        target_sources(snake_presentation PRIVATE src/web_platform.cpp)
        # Exceptions are used by catalog/settings validation. Reserve memory up
        # front so dense combat cannot trigger a WebAssembly heap-growth pause.
        target_compile_options(snake_core PUBLIC -fexceptions)
        target_link_options(snake_survivors PRIVATE
            -fexceptions -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
            -sINITIAL_MEMORY=268435456 -sSTACK_SIZE=8388608 -sALLOW_MEMORY_GROWTH=0
            -sEXIT_RUNTIME=0 -sENVIRONMENT=web -lidbfs.js
            "-sEXPORTED_RUNTIME_METHODS=['FS','addRunDependency','removeRunDependency']"
            "SHELL:--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html\""
            "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/data@/data\"")
        set_target_properties(snake_survivors PROPERTIES OUTPUT_NAME index SUFFIX .html)
        # Relink when an asset or the shell changes, even without a C++ edit.
        file(GLOB_RECURSE snake_web_assets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
        set_property(TARGET snake_survivors APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html" ${snake_web_assets})
    else()
        target_link_libraries(snake_survivors PRIVATE snake_test_api)
    endif()
    set_target_properties(snake_survivors PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>"
    )
    if(MSVC)
        target_compile_options(snake_survivors PRIVATE /W4 /permissive-)
        target_compile_options(snake_presentation PRIVATE /W4 /permissive-)
        set_property(TARGET snake_survivors PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:snake_survivors>")
    else()
        target_compile_options(snake_survivors PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(snake_presentation PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_dependencies(snake_survivors snake_assets)
    if(BUILD_TESTING)
        add_executable(snake_render_benchmark tests/render_benchmark.cpp)
        target_link_libraries(snake_render_benchmark PRIVATE snake_presentation)
        set_target_properties(snake_render_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
        add_dependencies(snake_render_benchmark snake_assets)
        add_executable(snake_death_benchmark tests/death_benchmark.cpp)
        target_link_libraries(snake_death_benchmark PRIVATE snake_presentation)
        set_target_properties(snake_death_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
        add_dependencies(snake_death_benchmark snake_assets)
        add_executable(snake_presentation_tests tests/presentation_tests.cpp)
        target_link_libraries(snake_presentation_tests PRIVATE snake_presentation)
        target_compile_definitions(snake_presentation_tests PRIVATE SNAKE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
            SNAKE_TEST_ARTIFACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/artifacts/gametest")
        set_target_properties(snake_presentation_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
        add_dependencies(snake_presentation_tests snake_assets)
        foreach(native_suite IN ITEMS world_raster display_settings settings_ui gui_text effect_art death_art)
            add_executable(snake_${native_suite}_tests tests/${native_suite}_tests.cpp)
            target_link_libraries(snake_${native_suite}_tests PRIVATE snake_presentation)
            if(native_suite STREQUAL "display_settings" AND NOT EMSCRIPTEN)
                target_link_libraries(snake_${native_suite}_tests PRIVATE glfw)
            endif()
            target_compile_definitions(snake_${native_suite}_tests PRIVATE SNAKE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
                SNAKE_TEST_ARTIFACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/artifacts/gametest")
            set_target_properties(snake_${native_suite}_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
            if(SNAKE_RUN_NATIVE_TESTS)
                add_test(NAME snake_${native_suite}_tests COMMAND snake_${native_suite}_tests)
                set_tests_properties(snake_${native_suite}_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 30 LABELS native)
            endif()
        endforeach()
        if(SNAKE_RUN_NATIVE_TESTS)
            add_test(NAME snake_presentation_tests COMMAND snake_presentation_tests)
            set_tests_properties(snake_presentation_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 30 LABELS native)
        endif()
    endif()
    if(EMSCRIPTEN)
        install(FILES "${CMAKE_BINARY_DIR}/bin/index.html" "${CMAKE_BINARY_DIR}/bin/index.js"
            "${CMAKE_BINARY_DIR}/bin/index.wasm" "${CMAKE_BINARY_DIR}/bin/index.data"
            DESTINATION . COMPONENT game)
    else()
        install(TARGETS snake_survivors RUNTIME DESTINATION . COMPONENT game)
        install(DIRECTORY data DESTINATION . COMPONENT game)
    endif()
    install(FILES README.md DESTINATION . COMPONENT game)
    install(FILES docs/THIRD_PARTY.md docs/DESIGN.md docs/REFERENCE_VISUALS.md docs/GAME_TEST_API.md docs/WEAPON_PARITY.md docs/VALIDATION.md docs/AUDIO.md docs/LOOT.md docs/MOVEMENT.md docs/GREY_SNAKE.md docs/SETTINGS.md docs/WEB.md docs/EFFECT_ART.md DESTINATION docs COMPONENT game)
    install(FILES third_party/nlohmann/LICENSE.MIT DESTINATION third_party/nlohmann COMPONENT game)
endif()

if(BUILD_TESTING)
    # Offline balance analysis; opt in explicitly, outside the gameplay path.
    add_executable(snake_balance_sim EXCLUDE_FROM_ALL tests/balance_sim.cpp)
    target_link_libraries(snake_balance_sim PRIVATE snake_core)
    set_target_properties(snake_balance_sim PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
    if(SNAKE_RUN_BALANCE_TESTS)
        add_custom_target(snake_balance_checks ALL DEPENDS snake_balance_sim)
        add_test(NAME snake_evolution_upgrades COMMAND snake_balance_sim
            --data "${CMAKE_CURRENT_SOURCE_DIR}/data" --evolution-check --profile all
            --seeds 5 --seconds 120 --assert-upgrades
            --output "${CMAKE_BINARY_DIR}/evolution-upgrades.json")
        set_tests_properties(snake_evolution_upgrades PROPERTIES TIMEOUT 180 LABELS balance)
    endif()

    add_executable(snake_core_tests tests/core_tests.cpp)
    target_link_libraries(snake_core_tests PRIVATE snake_core)
    target_compile_definitions(snake_core_tests PRIVATE SNAKE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_test(NAME snake_core_tests COMMAND snake_core_tests)
    if(NOT SNAKE_PYTHON_EXECUTABLE)
        find_package(Python3 3.10 QUIET COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            set(SNAKE_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
        endif()
    endif()
    if(SNAKE_PYTHON_EXECUTABLE)
        add_test(NAME snake_python_logic COMMAND "${SNAKE_PYTHON_EXECUTABLE}"
            -m game_tests.run --tier logic --exe "$<TARGET_FILE:snake_game_test>"
            --data "${CMAKE_CURRENT_SOURCE_DIR}/data" --json "${CMAKE_BINARY_DIR}/game-test-results.json")
        set_tests_properties(snake_python_logic PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            TIMEOUT 60 LABELS gametest)
        if(SNAKE_BUILD_GAME AND SNAKE_RUN_NATIVE_TESTS AND NOT EMSCRIPTEN)
            add_test(NAME snake_python_native_weapons COMMAND "${SNAKE_PYTHON_EXECUTABLE}"
                -m game_tests.run --native --exe "$<TARGET_FILE:snake_survivors>"
                --single test_weapon_functionality,test_evolution_functionality,test_weapon_interactions
                --data "${CMAKE_CURRENT_SOURCE_DIR}/data" --json "${CMAKE_BINARY_DIR}/native-weapon-results.json")
            set_tests_properties(snake_python_native_weapons PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60 LABELS "native;gametest;weapons")
            add_test(NAME snake_python_native_grey COMMAND "${SNAKE_PYTHON_EXECUTABLE}"
                -m game_tests.run --native --exe "$<TARGET_FILE:snake_survivors>"
                --single test_grey_snake,test_grey_snake_visual
                --data "${CMAKE_CURRENT_SOURCE_DIR}/data" --json "${CMAKE_BINARY_DIR}/native-grey-results.json")
            set_tests_properties(snake_python_native_grey PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60 LABELS "native;gametest")
        endif()
    endif()
    foreach(suite IN ITEMS enemy_grid grey_snake nest edge snake_geometry weapon_parity boss_loot audio render_motion settings death)
        set(suite_target snake_${suite}_tests)
        if(suite STREQUAL "snake_geometry")
            set(suite_target snake_geometry_tests)
        endif()
        add_executable(${suite_target} tests/${suite}_tests.cpp)
        target_link_libraries(${suite_target} PRIVATE snake_core)
        target_compile_definitions(${suite_target} PRIVATE SNAKE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
        add_test(NAME ${suite_target} COMMAND ${suite_target})
    endforeach()
    add_executable(snake_core_stress tests/stress.cpp)
    target_link_libraries(snake_core_stress PRIVATE snake_core)
    target_compile_definitions(snake_core_stress PRIVATE SNAKE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    if(SNAKE_RUN_STRESS_TESTS)
        add_test(NAME snake_core_stress COMMAND snake_core_stress)
        set_tests_properties(snake_core_stress PROPERTIES TIMEOUT 120 LABELS stress)
    endif()
endif()
