# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /Applications/CMake.app/Contents/bin/cmake

# The command to remove a file.
RM = /Applications/CMake.app/Contents/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build

# Include any dependencies generated for this target.
include CMakeFiles/cycle_detect.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/cycle_detect.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cycle_detect.dir/flags.make

CMakeFiles/cycle_detect.dir/main.cpp.o: CMakeFiles/cycle_detect.dir/flags.make
CMakeFiles/cycle_detect.dir/main.cpp.o: ../main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/cycle_detect.dir/main.cpp.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/cycle_detect.dir/main.cpp.o -c /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/main.cpp

CMakeFiles/cycle_detect.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cycle_detect.dir/main.cpp.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/main.cpp > CMakeFiles/cycle_detect.dir/main.cpp.i

CMakeFiles/cycle_detect.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cycle_detect.dir/main.cpp.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/main.cpp -o CMakeFiles/cycle_detect.dir/main.cpp.s

CMakeFiles/cycle_detect.dir/main.cpp.o.requires:

.PHONY : CMakeFiles/cycle_detect.dir/main.cpp.o.requires

CMakeFiles/cycle_detect.dir/main.cpp.o.provides: CMakeFiles/cycle_detect.dir/main.cpp.o.requires
	$(MAKE) -f CMakeFiles/cycle_detect.dir/build.make CMakeFiles/cycle_detect.dir/main.cpp.o.provides.build
.PHONY : CMakeFiles/cycle_detect.dir/main.cpp.o.provides

CMakeFiles/cycle_detect.dir/main.cpp.o.provides.build: CMakeFiles/cycle_detect.dir/main.cpp.o


CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o: CMakeFiles/cycle_detect.dir/flags.make
CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o: ../inc/perf_utils.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o -c /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/inc/perf_utils.cpp

CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/inc/perf_utils.cpp > CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.i

CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/inc/perf_utils.cpp -o CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.s

CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.requires:

.PHONY : CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.requires

CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.provides: CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.requires
	$(MAKE) -f CMakeFiles/cycle_detect.dir/build.make CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.provides.build
.PHONY : CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.provides

CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.provides.build: CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o


# Object files for target cycle_detect
cycle_detect_OBJECTS = \
"CMakeFiles/cycle_detect.dir/main.cpp.o" \
"CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o"

# External object files for target cycle_detect
cycle_detect_EXTERNAL_OBJECTS =

cycle_detect: CMakeFiles/cycle_detect.dir/main.cpp.o
cycle_detect: CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o
cycle_detect: CMakeFiles/cycle_detect.dir/build.make
cycle_detect: CMakeFiles/cycle_detect.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable cycle_detect"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cycle_detect.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cycle_detect.dir/build: cycle_detect

.PHONY : CMakeFiles/cycle_detect.dir/build

CMakeFiles/cycle_detect.dir/requires: CMakeFiles/cycle_detect.dir/main.cpp.o.requires
CMakeFiles/cycle_detect.dir/requires: CMakeFiles/cycle_detect.dir/inc/perf_utils.cpp.o.requires

.PHONY : CMakeFiles/cycle_detect.dir/requires

CMakeFiles/cycle_detect.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cycle_detect.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cycle_detect.dir/clean

CMakeFiles/cycle_detect.dir/depend:
	cd /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build /Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/cc_find/build/CMakeFiles/cycle_detect.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cycle_detect.dir/depend
