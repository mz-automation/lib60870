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

configure_file(certs/server_CA1_1.key server_CA1_1.key COPYONLY)
configure_file(certs/server_CA1_1.pem server_CA1_1.pem COPYONLY)
configure_file(certs/root_CA1.pem root_CA1.pem COPYONLY)

configure_file(certs/client_CA1_1.key client_CA1_1.key COPYONLY)
configure_file(certs/client_CA1_1.pem client_CA1_1.pem COPYONLY)

configure_file(certs/client_CA1_2.key client_CA1_2.key COPYONLY)
configure_file(certs/client_CA1_2.pem client_CA1_2.pem COPYONLY)

configure_file(certs/client_CA1_3.key client_CA1_3.key COPYONLY)
configure_file(certs/client_CA1_3.pem client_CA1_3.pem COPYONLY)

configure_file(certs/client_CA1_4.key client_CA1_4.key COPYONLY)
configure_file(certs/client_CA1_4.pem client_CA1_4.pem COPYONLY)

configure_file(certs/server_CA1_1_chain.pem server_CA1_1_chain.pem)

configure_file(certs/test.crl test.crl COPYONLY)

add_executable(tests
  ${tests_SRCS}
)

target_link_libraries(tests
    lib60870
)
