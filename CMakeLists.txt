cmake_minimum_required(VERSION 3.13)
project(NotBlockingQueue)

set(CMAKE_CXX_STANDARD 11)

add_executable(LockFreeQueue main.cpp LFQueue/HazardPointers.hpp LFQueue/MSQueue.hpp LFQueue/MSQueueTests.hpp)