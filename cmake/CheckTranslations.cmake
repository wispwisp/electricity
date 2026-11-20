# SPDX-License-Identifier: GPL-3.0-or-later
# Fails if any .ts file would have unfinished translations after extracting strings from the
# sources (ADR 0022). Usage: cmake -DLUPDATE=<lupdate> -DSOURCE_DIR=<dir> -DTS_DIR=<dir>
#                                  -DWORK_DIR=<dir> -P CheckTranslations.cmake
file(GLOB ts_files "${TS_DIR}/*.ts")
if(NOT ts_files)
    message(FATAL_ERROR "no .ts files in ${TS_DIR}")
endif()

# Work on copies so the tracked files are never rewritten.
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(failures "")
foreach(ts_file IN LISTS ts_files)
    get_filename_component(name "${ts_file}" NAME)
    file(COPY "${ts_file}" DESTINATION "${WORK_DIR}")
    set(options -silent)
    if(name MATCHES "_en\\.ts$")
        list(APPEND options -pluralonly)
    endif()
    execute_process(COMMAND "${LUPDATE}" ${options} "${SOURCE_DIR}" -ts "${WORK_DIR}/${name}"
                    COMMAND_ERROR_IS_FATAL ANY)
    file(READ "${WORK_DIR}/${name}" content)
    string(REGEX MATCHALL "type=\"unfinished\"" unfinished "${content}")
    list(LENGTH unfinished count)
    if(NOT count EQUAL 0)
        string(APPEND failures "${name}: ${count} unfinished translation(s)\n")
    endif()
endforeach()

if(failures)
    message(FATAL_ERROR "${failures}")
endif()
message(STATUS "All translations finished")
