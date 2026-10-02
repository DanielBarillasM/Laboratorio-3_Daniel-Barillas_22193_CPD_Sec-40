/*
 * UNIVERSIDAD DEL VALLE DE GUATEMALA - LABORATORIO 3
 * Ejercicio 1: Ping-Pong con MPI_Send y MPI_Recv.
 *
 * Objetivo del enunciado:
 *   1. Con dos procesos, rank 0 envía un entero a rank 1.
 *   2. Rank 1 lo devuelve; el intercambio se repite N veces.
 *   3. N se recibe desde la terminal y se mide el tiempo con MPI_Wtime.
 *   4. Para variar el tamaño se usa un arreglo de enteros; elements=1
 *      conserva exactamente el caso del entero solicitado.
 *
 * Uso: mpirun -np 2 ./build/ping_pong <rondas_N> <elementos>
 * Ejemplo: mpirun -np 2 ./build/ping_pong 1000 256
 *
 * La presentación visual ocurre DESPUÉS del tramo cronometrado. La última
 * línea sigue siendo CSV puro para construir la gráfica sin transcribirla.
 */
#include <stdio.h>   /* printf, puts y mensajes de error. */
#include <stdlib.h>  /* free y códigos EXIT_SUCCESS / EXIT_FAILURE. */

#include "common.h" /* Validación de argumentos, memoria y llamadas MPI. */
#include "visual.h" /* Colores, separadores y figuras para la terminal. */

/* Etiquetas distintas impiden confundir la ida con la devolución. */
enum { PING_TAG = 11, PONG_TAG = 12 };

/* Crear un patrón verificable sin depender de números aleatorios. */
static void fill_message(int *message, int elements)
{
    /* Cada posición tiene un valor conocido por ambos procesos. */
    for (int i = 0; i < elements; ++i) {
        message[i] = i % 1009;
    }
}

/* Comprobar que el dato devuelto conserva el patrón original. */
static void verify_echo(const int *message, int elements)
{
    for (int i = 0; i < elements; ++i) {
        /* Un valor distinto señala una transferencia incorrecta. */
        if (message[i] != i % 1009) {
            fprintf(stderr, "Ping-Pong: dato incorrecto en posición %d.\n", i);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }
}

/* Una ronda completa desde la perspectiva del emisor rank 0. */
static void round_as_origin(const int *send_buffer, int *receive_buffer,
                            int elements)
{
    /* El envío bloqueante entrega el mensaje a rank 1 con etiqueta PING. */
    LAB_MPI(MPI_Send(send_buffer, elements, MPI_INT, 1, PING_TAG,
                     MPI_COMM_WORLD));

    /* El origen espera el eco de rank 1 antes de iniciar otra ronda. */
    LAB_MPI(MPI_Recv(receive_buffer, elements, MPI_INT, 1, PONG_TAG,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE));
}

/* La misma ronda desde la perspectiva del receptor rank 1. */
static void round_as_echo(int *receive_buffer, int elements)
{
    /* Rank 1 primero recibe; de otro modo no tendría datos que devolver. */
    LAB_MPI(MPI_Recv(receive_buffer, elements, MPI_INT, 0, PING_TAG,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE));

    /* Se devuelve exactamente el buffer recibido, sin transformaciones. */
    LAB_MPI(MPI_Send(receive_buffer, elements, MPI_INT, 0, PONG_TAG,
                     MPI_COMM_WORLD));
}

/* Mostrar el esquema y los tiempos después de terminar el experimento. */
static void print_summary(int rounds, int elements, double total_seconds)
{
    /* Un entero MPI_INT ocupa sizeof(int) bytes en esta máquina. */
    const size_t bytes = (size_t)elements * sizeof(int);

    /* Dividir el total entre las N rondas produce la ida y vuelta media. */
    const double round_trip_us = total_seconds * 1e6 / rounds;

    /* La mitad del RTT aproxima una dirección solo si ambas son similares. */
    const double one_way_estimate_us = round_trip_us / 2.0;

    lab_banner("01", "PING-PONG", "Comunicación bloqueante | 2 procesos");
    lab_section("Ruta de una ronda");
    printf("%s  [RANK 0] ── PING, tag %d ──────────────▶ [RANK 1]\n",
           lab_color(LAB_ANSI_CIAN), PING_TAG);
    printf("  [RANK 0] ◀────────────── PONG, tag %d ── [RANK 1]%s\n\n",
           PONG_TAG, lab_color(LAB_ANSI_RESET));

    lab_section("Configuración y verificación");
    printf("  Rondas solicitadas : %d\n", rounds);
    printf("  Enteros por mensaje: %d\n", elements);
    printf("  Bytes por mensaje  : %zu\n", bytes);
    lab_bar("Rondas completas", 20);
    lab_success("El último mensaje regresó sin cambios.");

    lab_section("Medición con MPI_Wtime");
    printf("  Tiempo total         : %.9f s\n", total_seconds);
    printf("  Ida y vuelta media   : %.3f us\n", round_trip_us);
    printf("  Ida estimada (RTT/2) : %.3f us\n", one_way_estimate_us);
    printf("%s  Nota: la estimación RTT/2 supone trayectos simétricos.%s\n\n",
           lab_color(LAB_ANSI_AMARILLO), lab_color(LAB_ANSI_RESET));

    /* La cabecera explica el orden de columnas; la fila final queda limpia. */
    puts("tipo,elementos,bytes,rondas,total_s,ida_vuelta_us,ida_estimada_us");
    printf("ping_pong,%d,%zu,%d,%.9f,%.3f,%.3f\n", elements, bytes,
           rounds, total_seconds, round_trip_us, one_way_estimate_us);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    /* rank es el ID local; size es el número de procesos en COMM_WORLD. */
    int rank, size;

    /* El estudiante fija N y el tamaño del mensaje desde la terminal. */
    int rounds, elements;

    /* Rank 0 necesita dos buffers; rank 1 solo el de recepción y eco. */
    int *send_buffer = NULL;
    int *receive_buffer;

    /* Las marcas MPI_Wtime se restan siempre dentro del mismo rank. */
    double start = 0.0;
    double total = 0.0;

    /* Todos los procesos deben iniciar MPI antes de usar cualquier llamada. */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }

    /* Los errores vuelven como código para que LAB_MPI los describa. */
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));

    /* Consultar identidad y tamaño del comunicador creado por mpirun. */
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));

    /* El ejercicio exige exactamente dos ranks y exactamente dos argumentos. */
    lab_require_size(size, 2, 2, rank);
    lab_usage_if(argc != 3, rank,
                 "ping_pong <rondas_N> <elementos_por_mensaje>");

    /* Rechazar cero, negativos, texto y valores fuera del rango int. */
    rounds = lab_positive_int(argv[1], "rondas_N", rank);
    elements = lab_positive_int(argv[2], "elementos_por_mensaje", rank);

    /* Cada rank reserva espacio para el mensaje que recibe. */
    receive_buffer = lab_alloc_ints(elements, rank);

    if (rank == 0) {
        /* Solo el emisor necesita construir el patrón inicial. */
        send_buffer = lab_alloc_ints(elements, rank);
        fill_message(send_buffer, elements);
    }

    /* Una ronda de calentamiento no entra en la medición. */
    if (rank == 0) {
        round_as_origin(send_buffer, receive_buffer, elements);
    } else {
        round_as_echo(receive_buffer, elements);
    }

    /* La barrera inicia la parte medida con ambos ranks preparados. */
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));

    if (rank == 0) {
        /* Solo rank 0 mide el ciclo completo de ida y regreso. */
        start = MPI_Wtime();
        for (int round = 0; round < rounds; ++round) {
            round_as_origin(send_buffer, receive_buffer, elements);
        }
        total = MPI_Wtime() - start;

        /* La verificación se hace FUERA de la región cronometrada. */
        verify_echo(receive_buffer, elements);
        print_summary(rounds, elements, total);
    } else {
        /* Cada recepción de rank 1 corresponde a un envío de rank 0. */
        for (int round = 0; round < rounds; ++round) {
            round_as_echo(receive_buffer, elements);
        }
    }

    /* Liberar los buffers después de la última transferencia. */
    free(send_buffer); /* free(NULL) también es válido en rank 1. */
    free(receive_buffer);

    /* MPI_Finalize libera el entorno antes de salir del proceso. */
    lab_finalize();
    return EXIT_SUCCESS;
}
