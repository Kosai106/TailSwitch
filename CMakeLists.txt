cmake_minimum_required(VERSION 3.22)

project(TailSwitch VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets DBus)
find_package(KF6StatusNotifierItem 6.14 REQUIRED)
qt_standard_project_setup()

add_library(tailswitch_desktop STATIC
    src/kdeclipboard.cpp src/kdeclipboard.h
    src/traymenu.cpp src/traymenu.h
)
target_compile_features(tailswitch_desktop PUBLIC cxx_std_17)
target_include_directories(tailswitch_desktop PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(tailswitch_desktop PUBLIC
    Qt6::Widgets Qt6::DBus KF6::StatusNotifierItem)
# SteamOS Qt uses protected data symbols that cannot be accessed via ELF copy
# relocations. Ubuntu's default PIE alone is insufficient; use GOT-based PIC
# for this library and its executable/test consumers.
target_compile_options(tailswitch_desktop PUBLIC
    "$<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>")

qt_add_executable(tailswitch src/main.cpp)
target_link_libraries(tailswitch PRIVATE tailswitch_desktop)

foreach(target tailswitch tailswitch_desktop)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
    qt_add_executable(test_desktop tests/test_desktop.cpp)
    target_link_libraries(test_desktop PRIVATE tailswitch_desktop Qt6::Test)
    add_test(NAME desktop_behaviors
        COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:test_desktop>)
    set_tests_properties(desktop_behaviors PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )

    find_program(READELF_EXECUTABLE readelf REQUIRED)
    add_test(NAME no_copy_relocations
        COMMAND "${CMAKE_COMMAND}"
            "-DREADELF=${READELF_EXECUTABLE}"
            "-DEXECUTABLE=$<TARGET_FILE:tailswitch>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_no_copy_relocations.cmake")
    set_tests_properties(no_copy_relocations PROPERTIES TIMEOUT 10)

    add_test(NAME cli_help COMMAND tailswitch --help)
    set_tests_properties(cli_help PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "smoke-test"
        TIMEOUT 10
    )
endif()
