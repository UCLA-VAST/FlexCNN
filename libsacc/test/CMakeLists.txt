find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(libsacc_unit_test test.cpp)

target_link_libraries(libsacc_unit_test PUBLIC ${GTEST_BOTH_LIBRARIES})
target_link_libraries(libsacc_unit_test PUBLIC ${GTEST_LIBRARIES} pthread)

target_link_libraries(libsacc_unit_test PUBLIC sacc)

target_include_directories(libsacc_unit_test PUBLIC ${CMAKE_SOURCE_DIR}/inc/)

add_test(
    NAME libsacc_unit_test
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libsacc_unit_test
)

