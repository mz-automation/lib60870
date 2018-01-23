include_directories(
   .
)

set(example_SRCS
   multi_client_server.c
)

IF(WIN32)
set_source_files_properties(${example_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(WIN32)

add_executable(multi_client_server
  ${example_SRCS}
)

target_link_libraries(multi_client_server
    lib60870
)
