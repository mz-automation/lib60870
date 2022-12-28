include_directories(
   .
)

set(example_SRCS
   cs104_client_async.c
)

IF(WIN32)
set_source_files_properties(${example_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(WIN32)

add_executable(cs104_client_async
  ${example_SRCS}
)

target_link_libraries(cs104_client_async
    lib60870
)
