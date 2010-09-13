# Copyright 2010 Matthew Arsenault, Travis Desell, Dave Przybylo,
# Nathan Cole, Boleslaw Szymanski, Heidi Newberg, Carlos Varela, Malik
# Magdon-Ismail and Rensselaer Polytechnic Institute.

# This file is part of Milkway@Home.

# Milkyway@Home is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# Milkyway@Home is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Milkyway@Home.  If not, see <http://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 2.8)
project(milkyway)

enable_testing()


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

configure_file (
  "${PROJECT_INCLUDE_DIR}/milkyway_config.h.in"
  "${PROJECT_INCLUDE_DIR}/milkyway_config.h")


link_directories(${LIBRARY_OUTPUT_PATH})
include_directories(${PROJECT_INCLUDE_DIR})

set(CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")

set(mw_lib_src src/milkyway_util.c)

if(MILKYWAY_OPENCL)
  list(APPEND mw_lib_src src/show_cl_types.c
                         src/build_cl.c)
endif()

add_library(milkyway STATIC ${mw_lib_src})

set_target_properties(milkyway PROPERTIES
                      COMPILE_DEFINITIONS "MILKYWAY_MATH_COMPILATION")

