include_directories(
   .
)

set(example_SRCS
   simple_server.c
)

IF(WIN32)
set_source_files_properties(${example_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(WIN32)

add_executable(cs104_server
  ${example_SRCS}
)

target_link_libraries(cs104_server
    lib60870
)
