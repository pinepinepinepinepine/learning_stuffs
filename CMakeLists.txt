
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

# we're compiling all the files in a dir/folder, specify where it is.
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
#the actual executable we're creating (first param) from a .cpp file (second param) -- path is relative to where this CMakeLists.txt is present
add_executable(main ${SRC_FILES})

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
"C:/stb" # lazy
"C:/mSys_Stuff/ucrt64/include" # lazy
${CMAKE_SOURCE_DIR}/headers #cmakesourcedir is where cmakelists is present.
)

# Where we find libs (the actual code).
target_link_libraries(main PRIVATE
${VULKAN_SDK}/Lib/vulkan-1.lib
${SLANG}/lib/slang.lib
"C:/mSys_Stuff/ucrt64/lib/libglfw3.a" # lazy
)

# ripped it. idea is that we're running the 'COMMAND' every cmake build (executes it before we build main, giving us slang.spv)
function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)
  set (SHADER_SOURCES ${SHADERS_DIR}/shader.slang)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/slang.spv
          COMMAND cmd /c ${SHADERS_DIR}/compile.bat # for some reason the slang compiler below is... whatever.
          COMMAND "${SLANG}/bin/slangc.exe" ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/slang.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Graphic Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

add_slang_shader_target( shaders )
add_dependencies(main shaders)

function (add_slang_graphics_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)
  set (SHADER_SOURCES ${SHADERS_DIR}/particle_graphics.slang)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/particle_graphics.spv
          COMMAND cmd /c ${SHADERS_DIR}/compile.bat # for some reason the slang compiler below is... whatever.
          COMMAND "${SLANG}/bin/slangc.exe" ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/particle_graphics.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Graphic Particle Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/particle_graphics.spv)
endfunction()

add_slang_graphics_shader_target( compute_shaders_g )
add_dependencies(main compute_shaders_g)


function (add_slang_compute_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry compMain)
  set (SHADER_SOURCES ${SHADERS_DIR}/particle_compute.slang)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/particle_compute.spv
          COMMAND cmd /c ${SHADERS_DIR}/compile.bat # for some reason the slang compiler below is... whatever.
          COMMAND "${SLANG}/bin/slangc.exe" ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/particle_compute.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Compute Particle Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/particle_compute.spv)
endfunction()

add_slang_compute_shader_target( compute_shaders )
add_dependencies(main compute_shaders)


function (add_slang_wireframe_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)
  set (SHADER_SOURCES ${SHADERS_DIR}/wireframe.slang)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/wireframe.spv
          COMMAND cmd /c ${SHADERS_DIR}/compile.bat # for some reason the slang compiler below is... whatever.
          COMMAND "${SLANG}/bin/slangc.exe" ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/wireframe.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Wireframe Graphic Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/wireframe.spv)
endfunction()

add_slang_wireframe_shader_target( wireframe_shaders )
add_dependencies(main wireframe_shaders)

function (add_slang_shadow_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain)
  set (SHADER_SOURCES ${SHADERS_DIR}/shadow_screen_vertex.slang)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/shadow_screen_vertex.spv
          COMMAND cmd /c ${SHADERS_DIR}/compile.bat # for some reason the slang compiler below is... whatever.
          COMMAND "${SLANG}/bin/slangc.exe" ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/shadow_screen_vertex.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Vertex Shadow Graphic Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/shadow_screen_vertex.spv)
endfunction()

add_slang_shadow_shader_target( shadow_vert_shaders )
add_dependencies(main shadow_vert_shaders)


function (add_slang_shadow_frag_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry fragMain)
  set (SHADER_SOURCES ${SHADERS_DIR}/shadow_screen_fragment.slang)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/shadow_screen_fragment.spv
          COMMAND cmd /c ${SHADERS_DIR}/compile.bat # for some reason the slang compiler below is... whatever.
          COMMAND "${SLANG}/bin/slangc.exe" ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/shadow_screen_fragment.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Fragment Shadow Graphic Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/shadow_screen_fragment.spv)
endfunction()

add_slang_shadow_frag_shader_target( shadow_frag_shaders )
add_dependencies(main shadow_frag_shaders)


function (add_slang_shadow_off_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain)
  set (SHADER_SOURCES ${SHADERS_DIR}/shadow_offscreen_vertex.slang)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/shadow_offscreen_vertex.spv
          COMMAND cmd /c ${SHADERS_DIR}/compile.bat # for some reason the slang compiler below is... whatever.
          COMMAND "${SLANG}/bin/slangc.exe" ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/shadow_offscreen_vertex.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Offscreen Shadow Graphic Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/shadow_offscreen_vertex.spv)
endfunction()

add_slang_shadow_off_shader_target( shadow_off_shaders )
add_dependencies(main shadow_off_shaders)

#clean this garbage up to not be the same thing 100 times.