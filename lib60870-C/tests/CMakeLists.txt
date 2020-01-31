include_directories(
   ./unity
)

set(tests_SRCS
   all_tests.c
   unity/unity.c
)

IF(WIN32)
set_source_files_properties(${tests_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(WIN32)

configure_file(server-key.pem server-key.pem COPYONLY)
configure_file(client1-key.pem client1-key.pem COPYONLY)
configure_file(client1.cer client1.cer COPYONLY)
configure_file(client2.cer client2.cer COPYONLY)
configure_file(root.cer root.cer COPYONLY)
configure_file(server.cer server.cer COPYONLY)

add_executable(tests
  ${tests_SRCS}
)

target_link_libraries(tests
    lib60870
)
