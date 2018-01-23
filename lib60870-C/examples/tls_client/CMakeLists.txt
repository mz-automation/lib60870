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

add_executable(tls_client
  ${example_SRCS}
)

target_link_libraries(tls_client
    lib60870
)
