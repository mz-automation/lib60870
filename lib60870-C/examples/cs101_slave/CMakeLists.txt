include_directories(
   .
)

set(example_SRCS
   slave_example.c
)

IF(WIN32)
set_source_files_properties(${example_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(WIN32)

add_executable(cs101_slave
  ${example_SRCS}
)

target_link_libraries(cs101_slave
    lib60870
)
