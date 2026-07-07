enable_testing()

include(GoogleTest)

add_executable(configTest
    configTest.cpp)

target_link_libraries(configTest
    app
    com
    gtest_main)

gtest_discover_tests(configTest
    TEST_SUFFIX .configTest)

add_test(NAME testConfig COMMAND configTest)