# Build the app, using the HAL

include_directories(include)
file(GLOB MY_SOURCES "src/*.c")
# message(STATUS "Sources: ${MY_SOURCES}") #use  this to check what files are included in the build
add_executable(assignment_3 ${MY_SOURCES})

# ALSA support
find_package(ALSA REQUIRED)

# Make use of the HAL library
target_link_libraries(assignment_3 LINK_PRIVATE hal)

# Copy executable to final location assignment_3
add_custom_command(TARGET assignment_3 POST_BUILD 
   COMMAND "${CMAKE_COMMAND}" -E copy 
      "$<TARGET_FILE:assignment_3>"
      "$ENV{HOME}/ensc351/public/myApps/assignment_3" 

#we dont need to copy these because they will already be inside of the directory
#   COMMAND "${CMAKE_COMMAND}" -E copy_directory
#      "${CMAKE_SOURCE_DIR}/wave-files"
#      "$ENV{HOME}/ensc351/public/myApps/beatbox-wav-files"
   COMMENT "Copying ARM executable to public NFS directory")
