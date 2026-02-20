foreach (dll IN LISTS DLLs)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "${OUT_DIR}")
endforeach()
