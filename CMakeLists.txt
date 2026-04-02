
# tell cmake what version we require
cmake_minimum_required(VERSION 3.10)

# sack of shit cmake: randomly (LITERALLY ALL OF A SUDDEN TODAY) i have to EXPLICITLY (ASIDE FROM LITERALLY TELLING THE DIR WHEN BUILDING) SPECIFY WHERE THIS FUCKING PATH IS
# OTHERWISE IT PICKS UP THE "DEFAULT" (GARBAGE) COMPILER PATH
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_C_COMPILER $ENV{CC})

#name of the project we're using, not needed rn but yeah
project( vulkanStuff )

# specify the C++ standard -- put this before add_executable or it doesn't do anything.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE) # cmake guarantees the standard used -- doesn't automatically fallback to an older version

#the actual executable we're creating (first param) from a .cpp file (second param) -- path is relative to where this CMakeLists.txt is present
add_executable(main src/main.cpp)

# ${var} is set by set these, you could ditch these by replacing ever ${var} with $ENV{var}
set(VULKAN_SDK $ENV{VULKAN_SDK})
set(GLFW $ENV{GLFW})
set(TINY $ENV{TINY})
set(SLANG $ENV{SLANG})

# for some reason using \ for slang's env var returns.... C:slang/lib/slang.lib (???)
string(REPLACE "\\" "/" SLANG ${SLANG})

# Where we find header files.
target_include_directories(main PRIVATE
${VULKAN_SDK}/Include
${TINY}
${GLFW}
${SLANG}/include
"C:/mSys Stuff/ucrt64/include" # lazy
)

# Where we find libs (the actual code).
target_link_libraries(main PRIVATE
${VULKAN_SDK}/Lib/vulkan-1.lib
${SLANG}/lib/slang.lib
"C:/mSys Stuff/ucrt64/lib/libglfw3.a" # lazy
)
