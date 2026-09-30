#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <map>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>

struct Tarea {
    std::string id;
    std::string nombre;
    int tiempo_ms;
    std::vector<std::string> dependencias;
    std::vector<std::string> sucesores;
    bool completada = false;
    bool abortada = false;
    bool lanzada = false;
};

std::vector<Tarea> lista_tareas;
std::vector<pid_t> hijos_activos;
std::map<std::string, size_t> indice_por_id;
std::map<pid_t, size_t> tarea_por_pid;
volatile sig_atomic_t interrumpido = 0;

Tarea* buscar_tarea(const std::string& id) {
    auto it = indice_por_id.find(id);
    if (it == indice_por_id.end()) return nullptr;
    return &lista_tareas[it->second];
}

bool es_espacio(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string recortar(const std::string& s) {
    size_t ini = 0, fin = s.size();
    while (ini < fin && es_espacio(s[ini])) ini++;
    while (fin > ini && es_espacio(s[fin - 1])) fin--;
    return s.substr(ini, fin - ini);
}

bool id_valido(const std::string& id) {
    if (id.empty()) return false;
    for (char c : id) {
        bool letra = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        bool digito = (c >= '0' && c <= '9');
        if (!letra && !digito && c != '_' && c != '-') return false;
    }
    return true;
}

bool parsear_entero(const std::string& s, long& valor) {
    if (s.empty()) return false;
    valor = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        valor = valor * 10 + (c - '0');
        if (valor > 2000000) return false;
    }
    return true;
}

void capturar_ctrl_c(int) {
    interrumpido = 1;
}

void capturar_sigchld(int) {
}

void abortar_todo() {
    std::cout << "\n[SEREMI] Llego la inspeccion, abortando todas las actividades...\n";
    std::cout.flush();

    size_t en_ejecucion = hijos_activos.size();

    for (pid_t pid : hijos_activos) kill(pid, SIGTERM);

    for (pid_t pid : hijos_activos) {
        waitpid(pid, nullptr, 0);
        Tarea& t = lista_tareas[tarea_por_pid[pid]];
        t.abortada = true;
        std::cout << "[ABORTADA] Tarea " << t.id << " (" << t.nombre << ")\n";
    }
    hijos_activos.clear();
    tarea_por_pid.clear();

    size_t pendientes = 0;
    for (auto& t : lista_tareas) {
        if (!t.completada && !t.abortada) {
            t.abortada = true;
            pendientes++;
        }
    }

    std::cout << "[SEREMI] Actividades abortadas en ejecucion: " << en_ejecucion
              << ", pendientes canceladas: " << pendientes << "\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " plan.txt K\n";
        return 1;
    }

    long K_leido = 0;
    if (!parsear_entero(argv[2], K_leido) || K_leido < 1) {
        std::cerr << "Error: K debe ser un entero mayor o igual a 1 (recibido: '" << argv[2] << "')\n";
        return 1;
    }
    int K = (int)K_leido;
    std::cout << "Iniciando planificador con K = " << K << "\n";
    std::cout.flush();

    std::srand((unsigned)std::time(nullptr));

    struct sigaction accion_int;
    accion_int.sa_handler = capturar_ctrl_c;
    sigemptyset(&accion_int.sa_mask);
    accion_int.sa_flags = 0;
    sigaction(SIGINT, &accion_int, nullptr);

    struct sigaction accion_chld;
    accion_chld.sa_handler = capturar_sigchld;
    sigemptyset(&accion_chld.sa_mask);
    accion_chld.sa_flags = 0;
    sigaction(SIGCHLD, &accion_chld, nullptr);

    std::ifstream archivo(argv[1]);
    if (!archivo) {
        std::cerr << "Error: No se pudo abrir " << argv[1] << "\n";
        return 1;
    }

    std::string linea;
    int num_linea = 0;

    while (std::getline(archivo, linea)) {
        num_linea++;
        linea = recortar(linea);
        if (linea.empty()) continue;

        std::vector<std::string> campos;
        std::stringstream ss(linea);
        std::string campo;
        while (std::getline(ss, campo, ':')) campos.push_back(recortar(campo));

        if (campos.size() < 3 || campos.size() > 4) {
            std::cerr << "Error (linea " << num_linea << "): formato invalido, se esperaba "
                      << "ID : Nombre : tiempo_ms : dependencias\n";
            return 1;
        }

        const std::string& id = campos[0];
        const std::string& nombre = campos[1];

        if (!id_valido(id)) {
            std::cerr << "Error (linea " << num_linea << "): ID invalido '" << id << "'\n";
            return 1;
        }
        if (nombre.empty()) {
            std::cerr << "Error (linea " << num_linea << "): la actividad '" << id << "' no tiene nombre\n";
            return 1;
        }
        if (indice_por_id.count(id)) {
            std::cerr << "Error (linea " << num_linea << "): ID duplicado '" << id << "'\n";
            return 1;
        }

        int tiempo;
        if (campos[2].empty()) {
            tiempo = 100 + rand() % 4901;
        } else {
            long t = 0;
            if (!parsear_entero(campos[2], t)) {
                std::cerr << "Error (linea " << num_linea << "): tiempo invalido '" << campos[2] << "'\n";
                return 1;
            }
            tiempo = (int)t;
        }

        std::vector<std::string> deps;
        if (campos.size() == 4) {
            std::string lista;
            for (char c : campos[3]) if (c != '[' && c != ']') lista += c;
            std::stringstream sd(lista);
            std::string dep;
            while (std::getline(sd, dep, ',')) {
                dep = recortar(dep);
                if (dep.empty()) continue;
                if (dep == id) {
                    std::cerr << "Error (linea " << num_linea << "): la actividad '" << id
                              << "' depende de si misma\n";
                    return 1;
                }
                bool repetida = false;
                for (const auto& d : deps) if (d == dep) repetida = true;
                if (!repetida) deps.push_back(dep);
            }
        }

        indice_por_id[id] = lista_tareas.size();
        lista_tareas.push_back({id, nombre, tiempo, deps, {}, false, false, false});
    }
    archivo.close();

    if (lista_tareas.empty()) {
        std::cerr << "Error: el plan no contiene actividades\n";
        return 1;
    }

    for (auto& t : lista_tareas) {
        for (const auto& dep_id : t.dependencias) {
            Tarea* pred = buscar_tarea(dep_id);
            if (!pred) {
                std::cerr << "Error: la actividad '" << t.id << "' depende de '" << dep_id
                          << "', que no existe en el plan\n";
                return 1;
            }
            pred->sucesores.push_back(t.id);
        }
    }

    {
        std::vector<int> pendientes(lista_tareas.size());
        std::vector<size_t> cola;
        for (size_t i = 0; i < lista_tareas.size(); i++) {
            pendientes[i] = (int)lista_tareas[i].dependencias.size();
            if (pendientes[i] == 0) cola.push_back(i);
        }
        size_t visitadas = 0;
        while (visitadas < cola.size()) {
            size_t actual = cola[visitadas++];
            for (const auto& suc_id : lista_tareas[actual].sucesores) {
                size_t j = indice_por_id[suc_id];
                if (--pendientes[j] == 0) cola.push_back(j);
            }
        }
        if (visitadas != lista_tareas.size()) {
            std::cerr << "Error: el plan contiene un ciclo, no es un DAG valido\n";
            return 1;
        }
    }

    int tuberia[2];
    if (pipe(tuberia) == -1) {
        perror("Error al crear pipe");
        return 1;
    }

    sigset_t mascara_bloqueo, mascara_original, mascara_vacia;
    sigemptyset(&mascara_bloqueo);
    sigaddset(&mascara_bloqueo, SIGINT);
    sigaddset(&mascara_bloqueo, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mascara_bloqueo, &mascara_original);
    sigemptyset(&mascara_vacia);

    size_t tareas_terminadas_o_abortadas = 0;

    while (tareas_terminadas_o_abortadas < lista_tareas.size()) {

        if (interrumpido) {
            abortar_todo();
            return 130;
        }

        for (auto& tarea : lista_tareas) {
            if (interrumpido) break;
            if (tarea.completada || tarea.abortada || tarea.lanzada) continue;

            bool listas_para_ejecutar = true;
            for (const std::string& dep_id : tarea.dependencias) {
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

                    signal(SIGINT, SIG_IGN);
                    sigprocmask(SIG_SETMASK, &mascara_original, nullptr);

                    prctl(PR_SET_PDEATHSIG, SIGKILL);

                    if (getppid() == 1) {
                        exit(EXIT_FAILURE);
                    }

                    close(tuberia[0]);

                    std::cout << "[INICIO] Tarea " << tarea.id << " (" << tarea.nombre << ") corriendo...\n";

                    usleep(tarea.tiempo_ms * 1000);

                    int idx = (int)(&tarea - lista_tareas.data());
                    write(tuberia[1], &idx, sizeof(int));
                    close(tuberia[1]);

                    exit(EXIT_SUCCESS);
                }
                else {
                    hijos_activos.push_back(pid);
                    tarea_por_pid[pid] = (size_t)(&tarea - lista_tareas.data());
                    tarea.lanzada = true;
                }
            }
        }

        if (!hijos_activos.empty() && !interrumpido) {
            sigsuspend(&mascara_vacia);

            int estado;
            pid_t pid_terminado;
            while ((pid_terminado = waitpid(-1, &estado, WNOHANG)) > 0) {

                for (auto it = hijos_activos.begin(); it != hijos_activos.end(); ++it) {
                    if (*it == pid_terminado) {
                        hijos_activos.erase(it);
                        break;
                    }
                }
                size_t idx_terminada = tarea_por_pid[pid_terminado];
                tarea_por_pid.erase(pid_terminado);

                if (WIFEXITED(estado) && WEXITSTATUS(estado) == EXIT_SUCCESS) {
                    int idx_mensaje;
                    read(tuberia[0], &idx_mensaje, sizeof(int));
                    lista_tareas[idx_terminada].completada = true;
                    std::cout << "[EXITO] Tarea " << lista_tareas[idx_terminada].id << " finalizada correctamente.\n";
                }

                tareas_terminadas_o_abortadas++;
            }
        }

        usleep(10000);
    }

    close(tuberia[0]);
    close(tuberia[1]);

    std::cout << "Plan de ejecucion terminado.\n";
    return 0;
}
