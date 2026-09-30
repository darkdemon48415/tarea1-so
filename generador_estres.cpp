#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

int main() {
    std::ofstream out("plan.txt");
    if (!out) {
        std::cerr << "Error al crear plan.txt\n";
        return 1;
    }

    std::srand(std::time(nullptr));
    int total_actividades = 10000;

    // Escribir actividades
    for (int i = 1; i <= total_actividades; ++i) {
        // Algunas actividades no tendrán tiempo asignado para probar el parsing aleatorio
        if (i % 10 == 0) {
            out << i << ",Tarea_" << i << ",\n";
        } else {
            int ms = 100 + (std::rand() % 400); // Tiempos cortos para pruebas rápidas
            out << i << ",Tarea_" << i << "," << ms << "\n";
        }
    }

    out << "---\n"; // Separador de dependencias

    // Generar dependencias densas en forma de DAG lineal/paralelo por bloques
    for (int i = 1; i <= total_actividades - 5; ++i) {
        // Cada tarea i es prerrequisito de las siguientes capas para forzar concurrencia y dependencias
        out << i << "," << (i + 1) << "\n";
        if (i % 5 == 0) {
            out << i << "," << (i + 5) << "\n";
        }
    }

    std::cout << "Archivo plan.txt masivo generado con éxito.\n";
    return 0;
}
