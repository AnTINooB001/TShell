cmake_minimum_required(VERSION 3.27)
project(tsh C)

set(CMAKE_C_STANDARD 11)

add_executable(tsh main.c
        ls_macros.h
        text_format.h)
