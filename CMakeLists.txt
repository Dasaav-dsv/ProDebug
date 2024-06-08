cmake_minimum_required(VERSION 3.15.0)
project(ProDebug LANGUAGES CXX)

add_library("ProDebug" SHARED "dllmain.cpp")
set_target_properties("ProDebug" PROPERTIES CXX_STANDARD 20)