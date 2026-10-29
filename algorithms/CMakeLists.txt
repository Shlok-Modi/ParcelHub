cmake_minimum_required(VERSION 3.14)
project(ParcelHub CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)   # same optimisation level for every benchmark build
endif()
if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(parcelhub_core
  src/Graph.cpp
  src/BinaryHeap.cpp
  src/BinomialHeap.cpp
  src/FibonacciHeap.cpp
  src/HeapFactory.cpp
  src/Dijkstra.cpp
  src/Prim.cpp
  src/DispatchEngine.cpp)
target_include_directories(parcelhub_core PUBLIC include)

add_executable(parcelhub src/main.cpp)
target_link_libraries(parcelhub parcelhub_core)

enable_testing()
foreach(t heapTests crossHeapTests routingTests primTests dispatchTests)
  add_executable(${t} ../tests/${t}.cpp)
  target_link_libraries(${t} parcelhub_core)
  target_include_directories(${t} PRIVATE ../tests)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
