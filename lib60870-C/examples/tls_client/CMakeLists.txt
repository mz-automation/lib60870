include_directories(
   .
)

set(example_SRCS
   tls_client.c
)

IF(WIN32)
set_source_files_properties(${example_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(WIN32)

configure_file(client1-key.pem client1-key.pem COPYONLY)
configure_file(client1.cer client1.cer COPYONLY)
configure_file(root.cer root.cer COPYONLY)
configure_file(server.cer server.cer COPYONLY)

add_executable(tls_client
  ${example_SRCS}
)

target_link_libraries(tls_client
    lib60870
)
