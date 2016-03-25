project(io)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Iinclude -Wall -g")
include_directories(before include)

set(SRC_LIST rts_io_i2c.c rts_io_gpio.c rts_io_adc.c)
add_library(rtsio SHARED ${SRC_LIST})
target_link_libraries(rtsio rtsisp)

add_executable(io_example io_example.c)
target_link_libraries(io_example rtsio)

install(FILES include/rts_io_i2c.h DESTINATION include/uapi)
install(FILES include/rts_io_gpio.h DESTINATION include/uapi)
install(FILES include/rts_io_adc.h DESTINATION include/uapi)
install(TARGETS rtsio LIBRARY DESTINATION lib)
install(TARGETS io_example RUNTIME DESTINATION bin)
