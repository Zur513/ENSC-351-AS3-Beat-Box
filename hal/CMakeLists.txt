# CMakeList.txt for HAL
# Build a library (`hal`) which exposes the header files as "hal/*.h"

file(GLOB MY_SOURCES "src/*.c")

add_library(hal STATIC ${MY_SOURCES})

target_include_directories(hal PUBLIC include)

# The encoder reads its GPIO lines with libgpiod
target_link_libraries(hal PRIVATE gpiod)
