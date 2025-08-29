add_executable(chatter_tests test_main.c)

target_link_libraries(chatter_tests PRIVATE cmocka OpenSSL::SSL OpenSSL::Crypto)

target_include_directories(chatter_tests PRIVATE ${cmocka_SOURCE_DIR}/include)

add_test(NAME chatter_tests COMMAND chatter_tests)