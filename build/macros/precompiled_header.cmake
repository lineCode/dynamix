# MaiBo
# Copyright (c) 2015 Borislav Stanimirov
#
# Distributed under the MIT Software License
# See accompanying file LICENSE.txt or copy at
# http://opensource.org/licenses/MIT
#
# add_precompiled_header
#
# Sets a precompiled header for a given target
# Args:
# TARGET_NAME - Name of the target. Only valid after add_library or add_executable
# PRECOMPILED_HEADER - Header file to precompile
# PRECOMPILED_SOURCE - MSVC specific source to do the actual precompilation. Ignored on other platforms
#
# Example Usage
# add_executable(myproj
#   src/myproj.pch.h
#   src/myproj.pch.cpp
#   src/main.cpp
#   ...
#   src/z.cpp
#   )
# add_precompiled_header(myproj src/myproj.pch.h src/myproj.pch.cpp)
#
macro(add_precompiled_header TARGET_NAME PRECOMPILED_HEADER PRECOMPILED_SOURCE)
    get_filename_component(PRECOMPILED_HEADER_NAME ${PRECOMPILED_HEADER} NAME)

    if(MSVC)
        get_filename_component(PRECOMPILED_HEADER_PATH ${PRECOMPILED_HEADER} DIRECTORY)
        target_include_directories(${TARGET_NAME} PRIVATE ${PRECOMPILED_HEADER_PATH}) # fixes occasional IntelliSense glitches

        get_filename_component(PRECOMPILED_HEADER_WE ${PRECOMPILED_HEADER} NAME_WE)
        set(PRECOMPILED_BINARY "$(IntDir)/${PRECOMPILED_HEADER_WE}.pch")

        get_target_property(SOURCE_FILES ${TARGET_NAME} SOURCES)
        set(SOURCE_FILE_FOUND FALSE)
        foreach(SOURCE_FILE ${SOURCE_FILES})
            set(PCH_COMPILE_FLAGS "")
            if(SOURCE_FILE MATCHES \\.\(c|cc|cxx|cpp\)$)
                if(${PRECOMPILED_SOURCE} MATCHES ${SOURCE_FILE})
                    # Set source file to generate header
                    set_source_files_properties(
                        ${SOURCE_FILE}
                        PROPERTIES
                        COMPILE_FLAGS "/Yc\"${PRECOMPILED_HEADER_NAME}\" /Fp\"${PRECOMPILED_BINARY}\""
                        OBJECT_OUTPUTS "${PRECOMPILED_BINARY}")
                    set(SOURCE_FILE_FOUND TRUE)
                else()
                    # Set and automatically include precompiled header
                    set_source_files_properties(
                        ${SOURCE_FILE}
                        PROPERTIES
                        COMPILE_FLAGS "/Yu\"${PRECOMPILED_HEADER_NAME}\" /Fp\"${PRECOMPILED_BINARY}\" /FI\"${PRECOMPILED_HEADER_NAME}\""
                        OBJECT_DEPENDS "${PRECOMPILED_BINARY}")
                endif()
            endif()
        endforeach()
        if(NOT SOURCE_FILE_FOUND)
            message(FATAL_ERROR "A source file for ${PRECOMPILED_HEADER} was not found. Required for MSVC builds.")
        endif(NOT SOURCE_FILE_FOUND)
    elseif(CMAKE_GENERATOR STREQUAL Xcode)
        set_target_properties(
            ${TARGET_NAME}
            PROPERTIES
            XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${PRECOMPILED_HEADER}"
            XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"
            )
    elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Create and set output directory.
        set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PRECOMPILED_HEADER_NAME}.gch")
        make_directory(${OUTPUT_DIR})
        set(OUTPUT_NAME "${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME}.gch")

	# Export compiler flags via a generator to a response file
        set(PCH_FLAGS_FILE "${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME}.rsp")
        set(_include_directories "$<TARGET_PROPERTY:${TARGET_NAME},INCLUDE_DIRECTORIES>")
        set(_compile_definitions "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_DEFINITIONS>")
        set(_compile_flags "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_FLAGS>")
        set(_compile_options "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_OPTIONS>")
        set(_include_directories "$<$<BOOL:${_include_directories}>:-I$<JOIN:${_include_directories},\n-I>\n>")
        set(_compile_definitions "$<$<BOOL:${_compile_definitions}>:-D$<JOIN:${_compile_definitions},\n-D>\n>")
        set(_compile_flags "$<$<BOOL:${_compile_flags}>:$<JOIN:${_compile_flags},\n>\n>")
        set(_compile_options "$<$<BOOL:${_compile_options}>:$<JOIN:${_compile_options},\n>\n>")
        file(GENERATE OUTPUT "${PCH_FLAGS_FILE}" CONTENT "${_compile_definitions}${_include_directories}${_compile_flags}${_compile_options}\n")

        # Add a custom target for building the precompiled header.
        add_custom_command(
            OUTPUT ${OUTPUT_NAME}
            COMMAND ${CMAKE_CXX_COMPILER} @${PCH_FLAGS_FILE} -x c++-header -o ${OUTPUT_NAME} ${PRECOMPILED_HEADER}
            DEPENDS ${PRECOMPILED_HEADER})
        add_custom_target(${TARGET_NAME}_gch DEPENDS ${OUTPUT_NAME})
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_gch)

        # set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS "-include ${PRECOMPILED_HEADER_NAME} -Winvalid-pch")
        get_target_property(SOURCE_FILES ${TARGET_NAME} SOURCES)
        foreach(SOURCE_FILE ${SOURCE_FILES})
            set(PCH_COMPILE_FLAGS "")
            if(SOURCE_FILE MATCHES \\.\(c|cc|cxx|cpp\)$)
                get_source_file_property(SRC_COMPILE_FLAGS "${SOURCE_FILE}" COMPILE_FLAGS)
                set_source_files_properties(${SOURCE_FILE} PROPERTIES
                   COMPILE_FLAGS "${SRC_COMPILE_FLAGS} -include ${PRECOMPILED_HEADER_NAME} -Winvalid-pch"
                )
            endif()
        endforeach()
    else()
        message(FATAL_ERROR "Unknown generator for add_precompiled_header.")
    endif()
endmacro(add_precompiled_header)
