# An executable with no imported shared libraries has an empty runtime list.
# Avoid invoking `cmake -E copy_if_different` with no source arguments.
foreach(dll IN LISTS RUNTIME_DLLS)
  cmake_path(GET dll FILENAME name)
  file(COPY_FILE "${dll}" "${DESTINATION}/${name}" ONLY_IF_DIFFERENT)
endforeach()
