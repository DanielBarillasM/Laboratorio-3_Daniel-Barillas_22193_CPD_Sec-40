/*
 * UNIVERSIDAD DEL VALLE DE GUATEMALA - LABORATORIO 3
 * Ejercicio 3: recepción anticipada con MPI_Irecv y MPI_Test.
 *
 * Requisitos del enunciado:
 *   - Ejecutar con dos procesos.
 *   - Rank 1 inicia la recepción de un entero y realiza aritmética.
 *   - Rank 0 envía el dato mientras rank 1 trabaja.
 *   - Rank 1 consulta MPI_Test e imprime dato y trabajo al completar.
 *   - Repetir con mensajes de varios tamaños para estudiar el tiempo.
 *
 * Con elements=1 se recibe un entero. Para el experimento de tamaños,
 * elements>1 representa un arreglo de enteros con el mismo protocolo.
 *
 * Uso: mpirun -np 2 ./build/recepcion_anticipada <elementos> <lote>
 * Ejemplo: mpirun -np 2 ./build/recepcion_anticipada 4096 1000
 */
#include <stdint.h>  /* uint32_t: aritmética de ancho fijo y overflow definido. */
#include <stdio.h>   /* Resumen legible y mensajes de error. */
#include <stdlib.h>  /* free y códigos de salida. */

#include "common.h" /* LAB_MPI, lectura segura y reserva de memoria. */
#include "visual.h" /* Línea de tiempo y paneles de terminal. */

/* Diferenciar la transferencia del dato de la del tiempo del emisor. */
enum { DATA_TAG = 31, TIME_TAG = 32 };

/* Rank 0 genera un patrón determinista que rank 1 puede verificar. */
static void fill_message(int *message, int elements)
{
    for (int i = 0; i < elements; ++i) {
        message[i] = i % 1009;
    }
}

/* Verificar todo el mensaje y resumirlo con una suma para la salida. */
static unsigned long long verify_and_sum(const int *message, int elements)
{
    unsigned long long sum = 0;

    for (int i = 0; i < elements; ++i) {
        /* Comparar cada entero evita validar solo el primero. */
        if (message[i] != i % 1009) {
            fprintf(stderr, "Recepción anticipada: valor erróneo en %d.\n", i);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        sum += (unsigned int)message[i];
    }
    return sum;
}

/*
 * Un lote de trabajo aritmético local. El resultado se conserva y se
 * imprime, por lo que el compilador no puede descartar el cálculo.
 */
static uint32_t do_arithmetic(uint32_t current, int batch_size)
{
    for (int i = 0; i < batch_size; ++i) {
        /* uint32_t tiene aritmética módulo 2^32; no hay overflow indefinido. */
        current = current * 1664525u + 1013904223u + (uint32_t)i;
    }
    return current;
}

/* Rank 0 envía sin bloqueo y reporta cuánto tardó su petición de envío. */
static void sender(int *message, int elements)
{
    MPI_Request request;
    double start = MPI_Wtime();
    double send_time;

    /* Isend inicia la transferencia y devuelve inmediatamente una petición. */
    LAB_MPI(MPI_Isend(message, elements, MPI_INT, 1, DATA_TAG,
                      MPI_COMM_WORLD, &request));

    /* Wait es obligatorio antes de modificar o liberar el buffer enviado. */
    LAB_MPI(MPI_Wait(&request, MPI_STATUS_IGNORE));
    send_time = MPI_Wtime() - start;

    /* Este dato viaja FUERA de la región de tiempo del envío. */
    LAB_MPI(MPI_Send(&send_time, 1, MPI_DOUBLE, 1, TIME_TAG,
                     MPI_COMM_WORLD));
}

/* Salida visual del receptor: primero diagrama y resumen; al final CSV. */
static void print_summary(int elements, int batch_size,
                          unsigned long long operations,
                          unsigned long long checksum, uint32_t work_state,
                          int first_value, double receive_time,
                          double send_time)
{
    const size_t bytes = (size_t)elements * sizeof(int);

    lab_banner("03", "RECEPCIÓN ANTICIPADA",
               "Comunicación sin bloqueo | 2 procesos");
    lab_section("Línea de tiempo conceptual");
    printf("%s  RANK 0: [preparar] ── Isend ───────────────────────▶ dato\n",
           lab_color(LAB_ANSI_CIAN));
    printf("  RANK 1: [Irecv] ── [cálculo] ── [MPI_Test] ── [listo]%s\n\n",
           lab_color(LAB_ANSI_RESET));

    lab_section("Configuración");
    printf("  Enteros recibidos     : %d\n", elements);
    printf("  Bytes del mensaje     : %zu\n", bytes);
    printf("  Operaciones por test  : %d\n", batch_size);

    lab_section("Resultado verificado");
    lab_bar("Recepción", 20);
    lab_success("Todos los enteros coinciden con el patrón esperado.");
    printf("  Primer entero         : %d\n", first_value);
    printf("  Suma del mensaje      : %llu\n", checksum);
    printf("  Trabajo aritmético    : %llu operaciones\n", operations);
    printf("  Estado del cálculo    : %u\n", work_state);

    lab_section("Tiempo local de cada rank");
    printf("  Recepción + trabajo   : %.9f s (rank 1)\n", receive_time);
    printf("  Petición de envío     : %.9f s (rank 0)\n", send_time);
    printf("%s  Nota: recepción + trabajo no es latencia pura de red.%s\n\n",
           lab_color(LAB_ANSI_AMARILLO), lab_color(LAB_ANSI_RESET));

    puts("tipo,elementos,bytes,operaciones_por_test,operaciones_total,"
         "tiempo_recepcion_s,tiempo_envio_s,suma");
    printf("recepcion_anticipada,%d,%zu,%d,%llu,%.9f,%.9f,%llu\n",
           elements, bytes, batch_size, operations, receive_time,
           send_time, checksum);
}

/* Rank 1 combina trabajo útil con sondeos periódicos de la petición. */
static void receiver(int *message, int elements, int batch_size)
{
    MPI_Request request;
    int finished = 0;
    unsigned long long operations = 0;
    unsigned long long checksum;
    uint32_t work_state = 2166136261u;
    double start = MPI_Wtime();
    double receive_time, send_time;

    /* Publicar Irecv ANTES de empezar el cálculo permite solapamiento. */
    LAB_MPI(MPI_Irecv(message, elements, MPI_INT, 0, DATA_TAG,
                      MPI_COMM_WORLD, &request));

    do {
        /* Completar un lote de aritmética sin esperar por la red. */
        work_state = do_arithmetic(work_state, batch_size);
        operations += (unsigned int)batch_size;

        /* MPI_Test no bloquea: finished es 1 si la petición terminó. */
        LAB_MPI(MPI_Test(&request, &finished, MPI_STATUS_IGNORE));
    } while (!finished);

    /* Medir antes de verificar y antes de imprimir cualquier figura. */
    receive_time = MPI_Wtime() - start;

    /* Ya es seguro leer el buffer porque MPI_Test confirmó su finalización. */
    checksum = verify_and_sum(message, elements);

    /* Recibir el tiempo local de rank 0 sin restar relojes de dos procesos. */
    LAB_MPI(MPI_Recv(&send_time, 1, MPI_DOUBLE, 0, TIME_TAG,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE));

    print_summary(elements, batch_size, operations, checksum, work_state,
                  message[0], receive_time, send_time);
}

int main(int argc, char **argv)
{
    int rank, size, elements, batch_size;
    int *message;

    /* MPI_Init da acceso a COMM_WORLD y a las demás llamadas MPI. */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));

    /* El protocolo tiene un único emisor y un único receptor. */
    lab_require_size(size, 2, 2, rank);
    lab_usage_if(argc != 3, rank,
                 "recepcion_anticipada <elementos> <operaciones_por_test>");

    /* El usuario controla el tamaño del mensaje y del lote aritmético. */
    elements = lab_positive_int(argv[1], "elementos", rank);
    batch_size = lab_positive_int(argv[2], "operaciones_por_test", rank);

    /* Ambos ranks reservan un buffer del mismo tamaño. */
    message = lab_alloc_ints(elements, rank);
    if (rank == 0) {
        fill_message(message, elements);
    }

    /* Alinear el comienzo sin comparar marcas de tiempo entre ranks. */
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));

    if (rank == 0) {
        sender(message, elements);
    } else {
        receiver(message, elements, batch_size);
    }

    /* El buffer se libera después de MPI_Wait o de MPI_Test terminado. */
    free(message);
    lab_finalize();
    return EXIT_SUCCESS;
}
