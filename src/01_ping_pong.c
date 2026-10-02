/*
 * Parte 1, ejercicio 1: Ping-Pong con comunicación bloqueante.
 *
 * Rank 0 envía un mensaje a rank 1 y espera su devolución N veces.
 * Con elementos=1 se intercambia exactamente un entero, como pide el
 * enunciado. Con más elementos se experimenta con el tamaño del mensaje.
 *
 * Compilar: mpicc -std=c11 -O2 -Wall -Wextra -pedantic \
 *           src/01_ping_pong.c -o build/ping_pong
 * Ejecutar: mpirun -np 2 ./build/ping_pong 10000 256
 * Argumentos: <rondas N> <elementos enteros por mensaje>
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

enum { PING_TAG = 11, PONG_TAG = 12 };

static void fill_message(int *message, int elements)
{
    for (int i = 0; i < elements; ++i) {
        message[i] = i % 1009;
    }
}

static void verify_echo(const int *message, int elements)
{
    for (int i = 0; i < elements; ++i) {
        if (message[i] != i % 1009) {
            fprintf(stderr, "Ping-Pong: dato incorrecto en posición %d.\n", i);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv)
{
    int rank, size, rounds, elements;
    int *send_buffer = NULL;
    int *receive_buffer;
    double start = 0.0, total = 0.0;

    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));
    lab_require_size(size, 2, 2, rank);
    lab_usage_if(argc != 3, rank,
                 "ping_pong <rondas_N> <elementos_por_mensaje>");
    rounds = lab_positive_int(argv[1], "rondas_N", rank);
    elements = lab_positive_int(argv[2], "elementos_por_mensaje", rank);

    receive_buffer = lab_alloc_ints(elements, rank);
    if (rank == 0) {
        send_buffer = lab_alloc_ints(elements, rank);
        fill_message(send_buffer, elements);
    }

    /* Una ronda de calentamiento evita incluir la primera inicialización
       interna de la biblioteca MPI en la medición. */
    if (rank == 0) {
        LAB_MPI(MPI_Send(send_buffer, elements, MPI_INT, 1, PING_TAG,
                         MPI_COMM_WORLD));
        LAB_MPI(MPI_Recv(receive_buffer, elements, MPI_INT, 1, PONG_TAG,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE));
    } else {
        LAB_MPI(MPI_Recv(receive_buffer, elements, MPI_INT, 0, PING_TAG,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        LAB_MPI(MPI_Send(receive_buffer, elements, MPI_INT, 0, PONG_TAG,
                         MPI_COMM_WORLD));
    }

    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    if (rank == 0) {
        start = MPI_Wtime();
        for (int round = 0; round < rounds; ++round) {
            LAB_MPI(MPI_Send(send_buffer, elements, MPI_INT, 1, PING_TAG,
                             MPI_COMM_WORLD));
            LAB_MPI(MPI_Recv(receive_buffer, elements, MPI_INT, 1, PONG_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        }
        total = MPI_Wtime() - start;
        verify_echo(receive_buffer, elements);
    } else {
        for (int round = 0; round < rounds; ++round) {
            LAB_MPI(MPI_Recv(receive_buffer, elements, MPI_INT, 0, PING_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
            LAB_MPI(MPI_Send(receive_buffer, elements, MPI_INT, 0, PONG_TAG,
                             MPI_COMM_WORLD));
        }
    }

    if (rank == 0) {
        const double round_trip_us = total * 1e6 / rounds;
        printf("Ping-Pong correcto | rondas=%d | mensaje=%zu bytes\n",
               rounds, (size_t)elements * sizeof(int));
        printf("Tiempo total=%.9f s | ida y vuelta media=%.3f us\n",
               total, round_trip_us);
        puts("tipo,elementos,bytes,rondas,total_s,ida_vuelta_us,ida_estimada_us");
        printf("ping_pong,%d,%zu,%d,%.9f,%.3f,%.3f\n", elements,
               (size_t)elements * sizeof(int), rounds, total,
               round_trip_us, round_trip_us / 2.0);
        fflush(stdout);
    }

    free(send_buffer);
    free(receive_buffer);
    LAB_MPI(MPI_Finalize());
    return EXIT_SUCCESS;
}
