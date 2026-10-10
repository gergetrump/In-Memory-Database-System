# ---------------------------------------------------------------------------
# IMLAB
# ---------------------------------------------------------------------------

include(ExternalProject)
find_package(Git REQUIRED)

# Get fmt
ExternalProject_Add(
        fmt_src
        PREFIX "vendor/fmt"
        GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
        GIT_TAG 0c9fce2ffefecfdce794e1859584e25877b7b592
        TIMEOUT 10
        CMAKE_ARGS
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        -DFMT_TEST=OFF
        -DFMT_DOC=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/vendor/fmt
        #        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        UPDATE_COMMAND ""
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/libfmt.a
)

# Prepare fmt
ExternalProject_Get_Property(fmt_src install_dir)
set(FMT_INCLUDE_DIR ${install_dir}/include)
set(FMT_LIBRARY_PATH ${install_dir}/lib/libfmt.a)
file(MAKE_DIRECTORY ${FMT_INCLUDE_DIR})
add_library(fmt STATIC IMPORTED)
set_property(TARGET fmt PROPERTY IMPORTED_LOCATION ${FMT_LIBRARY_PATH})
set_property(TARGET fmt APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${FMT_INCLUDE_DIR})

# Dependencies
add_dependencies(fmt fmt_src)

