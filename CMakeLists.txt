cmake_minimum_required(VERSION 3.10)
project(Tarea_1 CXX)

set(CMAKE_CXX_STANDARD 17)

# Ejecutable 1: Tu planificador de tareas principal
add_executable(planificador main.cpp)

# Ejecutable 2: El generador de estrés (que tiene su propio main)
add_executable(generador_estres generador_estres.cpp)
