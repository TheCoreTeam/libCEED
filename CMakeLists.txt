cmake_minimum_required(VERSION 3.18)
project(libCEED LANGUAGES C CXX Fortran)

# --- Configuration Options ---
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(STATIC "Build static library" OFF)
if (STATIC)
    set(BUILD_SHARED_LIBS OFF)
endif ()

# --- Compiler Flags & Standards ---
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Mimic MARCHFLAG logic
include(CheckCCompilerFlag)
check_c_compiler_flag("-march=native" HAS_MARCH_NATIVE)
if (HAS_MARCH_NATIVE)
    add_compile_options("-march=native")
endif ()

# --- Directories ---
set(CEED_OBJDIR "${CMAKE_BINARY_DIR}/build")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Backend Dependencies Detection ---

# 1. libXSMM
set(XSMM_DIR "../libxsmm" CACHE PATH "Path to XSMM")
find_library(XSMM_LIB xsmm PATHS "${XSMM_DIR}/lib" NO_DEFAULT_PATH)
if (XSMM_LIB)
    message(STATUS "Found XSMM: ${XSMM_LIB}")
    include_directories("${XSMM_DIR}/include")
    list(APPEND CEED_EXTERNAL_LIBS ${XSMM_LIB} "blas")
    add_definitions(-DCEED_USE_XSMM)
endif ()

# 2. CUDA
option(CEED_ENABLE_CUDA "Enable CUDA backend" ON)
if (CEED_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit)
    if (CUDAToolkit_FOUND)
        message(STATUS "CUDA Backend Enabled")
        list(APPEND CEED_EXTERNAL_LIBS CUDA::cudart CUDA::cuda_driver CUDA::nvrtc CUDA::cublas)
    endif ()


    set(CUTLASS_ENABLE_LIBRARY OFF)
    set(CUTLASS_ENABLE_PROFILER OFF)
    set(CUTLASS_ENABLE_TESTS OFF)
    set(CUTLASS_ENABLE_EXAMPLES OFF)
    set(CUTLASS_ENABLE_TOOLS OFF)
    set(CUTLASS_ENABLE_HEADERS_ONLY ON)
    add_subdirectory(third_party/cutlass)
endif ()

# 3. HIP / ROCm
set(ROCM_DIR "/opt/rocm" CACHE PATH "Path to ROCm")
if (EXISTS "${ROCM_DIR}")
    find_package(hip REQUIRED PATHS "${ROCM_DIR}")
    message(STATUS "HIP Backend Enabled")
    list(APPEND CEED_EXTERNAL_LIBS hip::host hipblas)
endif ()

# --- Source Files Selection ---

# Interface and Gallery
file(GLOB CEED_SOURCES
        "interface/ceed*.c"
        "backends/*.c"
        "gallery/*.c"
        "gallery/*/ceed*.c"
)

list(REMOVE_ITEM CEED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/interface/ceed-jit-source-root-default.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/interface/ceed-jit-source-root-install.c"
)

option(CEED_USE_INSTALL_ROOT "Use the installation path for JIT sources" OFF)

if (CEED_USE_INSTALL_ROOT)
    list(APPEND CEED_SOURCES "interface/ceed-jit-source-root-install.c")
    set_source_files_properties(interface/ceed-jit-source-root-install.c PROPERTIES
            COMPILE_DEFINITIONS "CEED_JIT_SOURCE_ROOT_DEFAULT=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/\""
    )
    # TODO: fix
    set_source_files_properties(interface/ceed-jit-source-root-install.c PROPERTIES
            COMPILE_DEFINITIONS "CEED_JIT_CUTE_SOURCE_ROOT_DEFAULT=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/\""
    )
    set_property(SOURCE interface/ceed-jit-source-root-default.c APPEND PROPERTY
            COMPILE_DEFINITIONS "CEED_JIT_CUDA_SOURCE_ROOT_DEFAULT=\"${CUDAToolkit_INCLUDE_DIRS}\""
    )
else ()
    list(APPEND CEED_SOURCES "interface/ceed-jit-source-root-default.c")
    set_source_files_properties(interface/ceed-jit-source-root-default.c PROPERTIES
            COMPILE_DEFINITIONS "CEED_JIT_SOURCE_ROOT_DEFAULT=\"${CMAKE_CURRENT_SOURCE_DIR}/include/\""
    )
    set_property(SOURCE interface/ceed-jit-source-root-default.c APPEND PROPERTY
            COMPILE_DEFINITIONS "CEED_JIT_CUTE_SOURCE_ROOT_DEFAULT=\"${CMAKE_CURRENT_SOURCE_DIR}/third_party/cutlass/include\""
    )
    set_property(SOURCE interface/ceed-jit-source-root-default.c APPEND PROPERTY
            COMPILE_DEFINITIONS "CEED_JIT_CUDA_SOURCE_ROOT_DEFAULT=\"${CUDAToolkit_INCLUDE_DIRS}\""
    )
endif ()

# Filter out specific JIT/Interface files as per Makefile
list(FILTER CEED_SOURCES EXCLUDE REGEX "interface/ceed-(cuda|hip).c")

# CPU Backends (Standard)
file(GLOB REF_SOURCES "backends/ref/*.c")
file(GLOB BLOCKED_SOURCES "backends/blocked/*.c")
file(GLOB OPT_SOURCES "backends/opt/*.c")
list(APPEND CEED_SOURCES ${REF_SOURCES} ${BLOCKED_SOURCES} ${OPT_SOURCES})

# CUDA Backends
if (CUDAToolkit_FOUND)
    file(GLOB CUDA_SOURCES
            "interface/ceed-cuda.c"
            "backends/cuda/*.c" "backends/cuda/*.cpp"
            "backends/cuda-ref/*.c" "backends/cuda-ref/*.cpp"
            "backends/cuda-ref/kernels/*.cu"
            "backends/cuda-shared/*.c"
            "backends/cuda-gen/*.c" "backends/cuda-gen/*.cpp"
    )
    list(APPEND CEED_SOURCES ${CUDA_SOURCES})
endif ()

# --- Library Target ---

add_library(ceed ${CEED_SOURCES})
target_link_libraries(ceed PUBLIC ${CEED_EXTERNAL_LIBS} m nvidia::cutlass::cutlass)

target_compile_options(ceed PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr --extended-lambda>
)

# Handle Fortran underscores
if (CMAKE_Fortran_COMPILER_ID STREQUAL "GNU")
    target_compile_definitions(ceed PRIVATE UNDERSCORE=1)
endif ()

add_subdirectory(examples)

# --- Git Versioning ---
# Executes git describe to get the version string
execute_process(
        COMMAND git describe --always --tags --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# --- Build Configuration Metadata ---
# The Makefile captures a list of variables. In CMake, we can manually
# list the ones we want to bake into the binary for the `ceed-config.o` target.
set(CONFIG_VARS
        "CC" "CFLAGS" "CPPFLAGS" "LDFLAGS" "CEED_LDFLAGS" "CEED_LDLIBS"
)

set(BUILD_CONFIG_STRING "// Build Configuration:")
foreach (v ${CONFIG_VARS})
    set(BUILD_CONFIG_STRING "${BUILD_CONFIG_STRING}\\n${v} = ${${v}}")
endforeach ()

# Apply specific definitions to the ceed-config object file
set_source_files_properties(interface/ceed-config.c PROPERTIES
        COMPILE_DEFINITIONS "CEED_GIT_VERSION=\"${GIT_DESCRIBE}\";CEED_BUILD_CONFIGURATION=\"${BUILD_CONFIG_STRING}\""
)

# --- Installation ---

include(GNUInstallDirs)
install(TARGETS ceed
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install Headers
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
)

# --- Summary Output ---
message(STATUS "----------------------------------------")
message(STATUS " libCEED Configuration Summary")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER_ID}")
message(STATUS "  CUDA:         ${CUDAToolkit_FOUND}")
message(STATUS "  XSMM:         ${XSMM_LIB}")
message(STATUS "----------------------------------------")
