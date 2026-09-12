cmake_minimum_required(VERSION 3.22)

project(TailSwitch VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(tailswitch src/main.cpp)
target_compile_features(tailswitch PRIVATE cxx_std_17)
set_target_properties(tailswitch PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(tailswitch PRIVATE Qt6::Widgets)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tailswitch PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME cli_help COMMAND tailswitch --help)
    set_tests_properties(cli_help PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "smoke-test"
        TIMEOUT 10
    )
endif()
