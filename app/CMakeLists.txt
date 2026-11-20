# Build the app, using the HAL

include_directories(include)
file(GLOB MY_SOURCES "src/*.c")
# message(STATUS "Sources: ${MY_SOURCES}") #use  this to check what files are included in the build
add_executable(beatbox ${MY_SOURCES})

# ALSA support
find_package(ALSA REQUIRED)

# Make use of the HAL library
target_link_libraries(beatbox LINK_PRIVATE hal asound) #need to link BOTH hal and asound alongsiide eachother

# Copy executable to final location beatbox
add_custom_command(TARGET beatbox POST_BUILD 
   COMMAND "${CMAKE_COMMAND}" -E copy 
      "$<TARGET_FILE:beatbox>"
      "$ENV{HOME}/ensc351/public/myApps/beatbox" 
   COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_SOURCE_DIR}/wave-files"
      "$ENV{HOME}/ensc351/public/myApps/beatbox-wav-files"
   COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_SOURCE_DIR}/as3-server"
      "$ENV{HOME}/ensc351/public/myApps/beatbox-server-copy"
   COMMENT "Copying beatbox, wave files, and server to public NFS directory")
