include_directories(
   .
)

set(tests_SRCS
   test_client.c
)

add_executable(test_client
  ${tests_SRCS}
)

target_link_libraries(test_client
    der_scheduler
    m
)