function(archive_target TARGET_DIR TARGET VERSION FLAVOR)
  execute_process(COMMAND objdump -f ${TARGET} OUTPUT_VARIABLE OBJDUMP ERROR_VARIABLE OBJDUMP
    WORKING_DIRECTORY ${TARGET_DIR}
  )
  string(REGEX REPLACE ".*architecture: ([^,]+).*" "\\1" ARCH ${OBJDUMP})
  string(REGEX REPLACE ".*:" "" ARCH ${ARCH})
  string(REGEX REPLACE "\\.[^.]*$" "" TARGET_NAME ${TARGET})
  if(WIN32)
    find_program(SEVENZIP 7z)
    if(NOT SEVENZIP STREQUAL "SEVENZIP-NOTFOUND")
      set(ARCHIVE "${TARGET_NAME}-${VERSION}-windows-${FLAVOR}-${ARCH}.zip")
      message(STATUS "Creating ${ARCHIVE}")
      execute_process(
        COMMAND ${SEVENZIP} a ${ARCHIVE} ${TARGET} -mx9 -bso0
        WORKING_DIRECTORY ${TARGET_DIR}
      )
    else()
      message(WARNING "7z not found, skipping release archive creation")
    endif()
  # todo: other platforms?
  else()
    set(ARCHIVE "${TARGET_NAME}-${VERSION}-linux-${FLAVOR}-${ARCH}.tar.gz")
    message(STATUS "Creating ${ARCHIVE}")
    execute_process(
      COMMAND tar czf ${ARCHIVE} ${TARGET} --owner 0 --group 0
      WORKING_DIRECTORY ${TARGET_DIR}
    )
  endif()
endfunction()
