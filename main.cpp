#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>

// Estructura básica para cada tarea (Fase 1)
struct Tarea {
    int id;
    std::string nombre;
    int tiempo_ms;
    std::vector<int> dependencias;
    std::vector<int> sucesores;
    bool completada = false;
    bool abortada = false;
};

// Variables globales accesibles por el manejador de señales
std::vector<Tarea> lista_tareas;
std::vector<pid_t> hijos_activos;

// Función para buscar una tarea por su ID
Tarea* buscar_tarea(int id) {
    for (auto& t : lista_tareas) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

// Manejador del Ctrl+C modificado (Fase 3)
void capturar_ctrl_c(int sig) {
    std::cout << "\n[Ctrl+C] Saliendo del planificador principal...\n";
    std::cout << "[INFO] Los procesos hijos mueren automáticamente gracias al Kernel.\n";
    // Al salir el padre con exit(), el sistema operativo mata a los hijos gracias a prctl()
    exit(sig);
}

int main(int argc, char* argv[]) {
    // Configurar límite de concurrencia K
    int K = (argc > 1) ? std::atoi(argv[1]) : 3;
    std::cout << "Iniciando planificador con K = " << K << "\n";

    // Registrar el manejador de Ctrl+C
    signal(SIGINT, capturar_ctrl_c);

    // 1. LEER EL ARCHIVO PLAN.TXT (Fase 1)
    std::ifstream archivo("plan.txt");
    if (!archivo) {
        std::cerr << "Error: No se pudo abrir plan.txt\n";
        return 1;
    }

    std::string linea;
    bool modo_dependencias = false;

    while (std::getline(archivo, linea)) {
        if (linea.empty()) continue;
        if (linea.substr(0, 3) == "---") {
            modo_dependencias = true;
            continue;
        }

        std::stringstream ss(linea);
        if (!modo_dependencias) {
            std::string id_str, nombre, tiempo_str;
            std::getline(ss, id_str, ',');
            std::getline(ss, nombre, ',');
            std::getline(ss, tiempo_str);

            int id = std::stoi(id_str);
            int tiempo = tiempo_str.empty() ? (100 + rand() % 4900) : std::stoi(tiempo_str);

            lista_tareas.push_back({id, nombre, tiempo, {}, {}, false, false});
        } else {
            std::string pred_str, suc_str;
            std::getline(ss, pred_str, ',');
            std::getline(ss, suc_str);

            int pred = std::stoi(pred_str);
            int suc = std::stoi(suc_str);

            Tarea* t_suc = buscar_tarea(suc);
            Tarea* t_pred = buscar_tarea(pred);
            if (t_suc && t_pred) {
                t_suc->dependencias.push_back(pred);
                t_pred->sucesores.push_back(suc);
            }
        }
    }
    archivo.close();

    // 2. CREAR TUBERÍA / PIPE DE COMUNICACIÓN (Fase 2)
    int tuberia[2];
    if (pipe(tuberia) == -1) {
        perror("Error al crear pipe");
        return 1;
    }

    // 3. BUCLE PRINCIPAL DE EJECUCIÓN
    size_t tareas_terminadas_o_abortadas = 0;

    while (tareas_terminadas_o_abortadas < lista_tareas.size()) {

        for (auto& tarea : lista_tareas) {
            if (tarea.completada || tarea.abortada) continue;

            bool listas_para_ejecutar = true;
            for (int dep_id : tarea.dependencias) {
                Tarea* dep = buscar_tarea(dep_id);
                if (dep && !dep->completada) {
                    listas_para_ejecutar = false;
                    break;
                }
            }

            if (listas_para_ejecutar && (int)hijos_activos.size() < K) {

                pid_t pid = fork();

                if (pid < 0) {
                    perror("Error en fork");
                    return 1;
                }

                if (pid == 0) {

                    prctl(PR_SET_PDEATHSIG, SIGKILL);

                    if (getppid() == 1) {
                        exit(EXIT_FAILURE);
                    }

                    close(tuberia[0]);

                    std::cout << "[INICIO] Tarea " << tarea.id << " (" << tarea.nombre << ") corriendo...\n";

                    // Simular el tiempo de trabajo
                    usleep(tarea.tiempo_ms * 1000);

                    // Notificar éxito enviando el ID de la tarea por el pipe
                    write(tuberia[1], &tarea.id, sizeof(int));
                    close(tuberia[1]);

                    exit(EXIT_SUCCESS);
                }
                else {
                    hijos_activos.push_back(pid);
                    tarea.completada = true;
                }
            }
        }

        // El Padre espera a que algún hijo termine
        if (!hijos_activos.empty()) {
            int estado;
            pid_t pid_terminado = waitpid(-1, &estado, 0);

            if (pid_terminado > 0) {

                for (auto it = hijos_activos.begin(); it != hijos_activos.end(); ++it) {
                    if (*it == pid_terminado) {
                        hijos_activos.erase(it);
                        break;
                    }
                }

                if (WIFEXITED(estado) && WEXITSTATUS(estado) == EXIT_SUCCESS) {
                    int id_exitoso;
                    read(tuberia[0], &id_exitoso, sizeof(int));
                    std::cout << "[ÉXITO] Tarea " << id_exitoso << " finalizada correctamente.\n";
                }

                tareas_terminadas_o_abortadas++;
            }
        }

        usleep(10000);
    }

    close(tuberia[0]);
    close(tuberia[1]);

    std::cout << "Plan de ejecución terminado.\n";
    return 0;
}