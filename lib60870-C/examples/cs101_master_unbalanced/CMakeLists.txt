include_directories(
   .
)

set(example_SRCS
   master_example.c
)

IF(WIN32)
set_source_files_properties(${example_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(WIN32)

add_executable(cs101_master_unbalanced
  ${example_SRCS}
)

target_link_libraries(cs101_master_unbalanced
    lib60870
)
