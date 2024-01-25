#=========================================================================================
# (C) (or copyright) 2020. Triad National Security, LLC. All rights reserved.
#
# This program was produced under U.S. Government contract 89233218CNA000001 for Los
# Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
# for the U.S. Department of Energy/National Nuclear Security Administration. All rights
# in the program are reserved by Triad National Security, LLC, and the U.S. Department
# of Energy/National Nuclear Security Administration. The Government is granted for
# itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
# license in this material to reproduce, prepare derivative works, distribute copies to
# the public, perform publicly and display publicly, and to permit others to do so.
#=========================================================================================

set (IDIR "")
set (LDIR "")
set (LIBMEM "")
set (MEMDEF "")

get_property(DRIVER_LIST GLOBAL PROPERTY DRIVERS_USED_IN_TESTS)
if( "burgers-benchmark" IN_LIST DRIVER_LIST OR NOT PARTHENON_DISABLE_EXAMPLES)

  option( USE_MEMORY_RECORD "Include memory recorder library" OFF )
  if(USE_MEMORY_RECORD)
    set( LIBMEM "-lmemory_recorder" )
    if(NOT DEFINED MEMORY_RECORD_PREFIX)
      set(MEMORY_RECORD_PREFIX $ENV{MEMREC_ROOT}) # NAME FROM THE MODULEFILE
    endif()
    if(NOT DEFINED MEMORY_RECORD_PREFIX) #IF STILL NOT DEFINED.
      set( MEMORY_RECORD_PREFIX $ENV{HOME}/.local)
      message( STATUS "SETTING MEMORY_RECORD_PREFIX TO DEFAULT:  ${MEMORY_RECORD_PREFIX}")
      message( STATUS "IF THIS IS WRONG, PROVIDE ON CL")
      message( STATUS "NOTE: ${MEMORY_RECORD_PREFIX}/lib MUST BE IN LD_LIBRARY_PATH AT RUNTIME.")
    endif()

    set( IDIR ${MEMORY_RECORD_PREFIX}/include )
    set (MEMDEF "USE_MEMORY_RECORD")

    set( CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE )
    set( LDIR ${MEMORY_RECORD_PREFIX}/lib )
    message( STATUS "MEMORY_RECORD LOCATION: ${MEMORY_RECORD_PREFIX}")
    message( STATUS "LINKED LIBS: ${LIBMEM}")
  endif()
    
  add_executable(
    burgers-benchmark
    burgers_driver.cpp
    burgers_driver.hpp
    burgers_package.cpp
    burgers_package.hpp
    main.cpp
    parthenon_app_inputs.cpp
    recon.hpp
    )

  set_property(SOURCE main.cpp PROPERTY COMPILE_DEFINITIONS ${MEMDEF})
  target_link_libraries( burgers-benchmark PRIVATE Parthenon::parthenon ${LIBMEM} )
  target_include_directories( burgers-benchmark PRIVATE ${IDIR} )
  target_link_directories( burgers-benchmark PRIVATE ${LDIR} )
  lint_target(burgers-benchmark)
endif()
