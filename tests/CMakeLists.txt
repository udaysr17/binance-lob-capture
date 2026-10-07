add_executable(unit_tests
  test_main.cpp
  test_decimal.cpp
  test_csv.cpp
)
target_link_libraries(unit_tests PRIVATE binance_core)
set_target_properties(unit_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

add_custom_target(tests DEPENDS unit_tests)
add_test(NAME unit_tests COMMAND unit_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
