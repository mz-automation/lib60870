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

add_executable(tests
  ${tests_SRCS}
)

target_link_libraries(tests
    lib60870
)
