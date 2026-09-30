# Tarea 1: Planificador de Tareas Concurrente (DAG)

## Integrantes
* Eylan Acuña - 21636001K
* Eric Fernandez - [RUT 2]

---

## Descripción de Funciones Implementadas

El sistema se compone de dos programas independientes escritos en C++17:

1. **Planificador (`main.cpp`)**:
   * **Parseo y Validación del Plan (`plan.txt`)**: Lee y valida cada línea del archivo con formato `ID : Nombre : tiempo_ms : dependencias`. Aplica validaciones estrictas de sintaxis, IDs no repetidos, nombres no vacíos y generación aleatoria de tiempo cuando se omite.
   * **Modelado del Grafo Dirigido Acíclico (DAG)**: Representa las relaciones de dependencia entre actividades y ejecuta una ordenación topológica previa para detectar y rechazar ciclos en el plan.
   * **Creación y Control de Procesos**: Ejecuta las tareas concurrentemente usando únicamente llamadas a sistema de procesos (`fork`), respetando el límite máximo de $K$ procesos concurrentes especificado por parámetro.
   * **Manejo de Señales e Interrupción Limpia**: Captura `SIGINT` (Ctrl+C) enviando `SIGTERM` a todos los hijos activos para abortar la ejecución inmediatamente, reportando las tareas en ejecución y pendientes canceladas. Además, maneja `SIGCHLD` para sincronización.
   * **Protección ante Procesos Huérfanos**: Utiliza `prctl(PR_SET_PDEATHSIG, SIGKILL)` en cada proceso hijo para asegurar su eliminación automática si el proceso padre finaliza de forma imprevista.

2. **Generador de Estrés (`generador_estres.cpp`)**:
   * Genera de forma automatizada un archivo `plan.txt` masivo con $10\,000$ actividades enlazadas mediante dependencias para someter al planificador a pruebas de alta carga y concurrencia.
