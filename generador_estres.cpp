#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <vector>

int main() {
    std::ofstream out("plan.txt");
    if (!out) {
        std::cerr << "Error al crear plan.txt\n";
        return 1;
    }

    std::srand(std::time(nullptr));
    int total_actividades = 10000;

    std::vector<std::vector<int>> deps(total_actividades + 1);
    for (int i = 1; i <= total_actividades - 5; ++i) {
        deps[i + 1].push_back(i);
        if (i % 5 == 0) {
            deps[i + 5].push_back(i);
        }
    }

    for (int i = 1; i <= total_actividades; ++i) {
        out << i << " : Tarea_" << i << " : ";
        if (i % 10 != 0) {
            int ms = 100 + (std::rand() % 400);
            out << ms;
        }
        out << " : ";
        for (size_t j = 0; j < deps[i].size(); ++j) {
            if (j > 0) out << ", ";
            out << deps[i][j];
        }
        out << "\n";
    }

    std::cout << "Archivo plan.txt masivo generado con exito.\n";
    return 0;
}
