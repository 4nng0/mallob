
# Add MaxSAT-specific sources to main Mallob executable
set(SATWITHPRE_MALLOB_SOURCES 
    src/app/sat/solvers/kissat.cpp 
    src/app/sat/solvers/lingeling.cpp 
    src/app/sat/solvers/portfolio_solver_interface.cpp
    )
set(MALLOB_COREPLUSCOMM_SOURCES 
    ${MALLOB_COREPLUSCOMM_SOURCES} 
    ${SATWITHPRE_MALLOB_SOURCES} 
    CACHE INTERNAL "")

#message("commons+SAT sources: ${BASE_SOURCES}") # Use to debug

# Include external libraries as necessary

# Satsuma utility library (prebuilt static)


if(MALLOB_USE_SATSUMA)
    add_definitions(-DMALLOB_USE_SATSUMA=1)
    
    set(BASE_LINK_DIRS
        ${BASE_LINK_DIRS}
        ${CMAKE_SOURCE_DIR}/lib/satsuma
        CACHE INTERNAL ""
    )

    set(BASE_LIBS
        ${BASE_LIBS}
        satsuma
        CACHE INTERNAL ""
    )

    set(BASE_INCLUDES
        ${BASE_INCLUDES}
        ${CMAKE_SOURCE_DIR}/lib/satsuma/include
        CACHE INTERNAL ""
    )
else()
    add_definitions(-DMALLOB_USE_SATSUMA=0)
endif()

# Add unit tests: for each $arg there must be a standalone cpp file under "test/test_${arg}.cpp".
# ...

new_test(satsuma_payload "${BASE_INCLUDES}" "mallob_corepluscomm;mallob_sat_subproc")
