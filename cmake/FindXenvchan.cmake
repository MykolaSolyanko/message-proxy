# FindXenvchan.cmake

# Try to find Xenvchan library
# Once done this will define
#  XENVCHAN_FOUND - System has Xenvchan
#  XENVCHAN_INCLUDE_DIRS - The Xenvchan include directories
#  XENVCHAN_LIBRARIES - The libraries needed to use Xenvchan

find_package(PkgConfig QUIET)
pkg_check_modules(PC_XENVCHAN QUIET xenvchan)

find_path(
    XENVCHAN_INCLUDE_DIR
    NAMES libxenvchan.h
    PATHS ${PC_XENVCHAN_INCLUDE_DIRS}
    PATH_SUFFIXES xen
)

find_library(
    XENVCHAN_LIBRARY
    NAMES xenvchan
    PATHS ${PC_XENVCHAN_LIBRARY_DIRS}
)

set(XENVCHAN_VERSION ${PC_XENVCHAN_VERSION})

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set XENVCHAN_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(
    Xenvchan
    REQUIRED_VARS XENVCHAN_LIBRARY XENVCHAN_INCLUDE_DIR
    VERSION_VAR XENVCHAN_VERSION
)

if(XENVCHAN_FOUND)
    set(XENVCHAN_LIBRARIES ${XENVCHAN_LIBRARY})
    set(XENVCHAN_INCLUDE_DIRS ${XENVCHAN_INCLUDE_DIR})
    if(NOT TARGET Xenvchan::Xenvchan)
        add_library(Xenvchan::Xenvchan UNKNOWN IMPORTED)
        set_target_properties(
            Xenvchan::Xenvchan PROPERTIES IMPORTED_LOCATION "${XENVCHAN_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES
                                                                                  "${XENVCHAN_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(XENVCHAN_INCLUDE_DIR XENVCHAN_LIBRARY)
