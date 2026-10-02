/*
 * Parte 2, ejercicio 1: recepción anticipada sin bloqueo.
 *
 * Rank 1 publica MPI_Irecv antes de realizar trabajo aritmético. Tras cada
 * lote de operaciones llama a MPI_Test para averiguar si llegó el mensaje.
 * Rank 0 utiliza MPI_Isend y espera antes de liberar su buffer. El caso
 * elementos=1 corresponde al entero pedido en el enunciado; los demás
 * tamaños permiten analizar la tendencia de tiempo.
 *
 * Ejecutar: mpirun -np 2 ./build/recepcion_anticipada 256 1000
 * Argumentos: <elementos enteros> <operaciones por consulta MPI_Test>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

enum { DATA_TAG = 31, TIME_TAG = 32 };

static void fill_message(int *message, int elements)
{
    for (int i = 0; i < elements; ++i) {
        message[i] = i % 1009;
    }
}

static unsigned long long verify_and_sum(const int *message, int elements)
{
    unsigned long long sum = 0;

    for (int i = 0; i < elements; ++i) {
        if (message[i] != i % 1009) {
            fprintf(stderr, "Recepción anticipada: valor erróneo en %d.\n", i);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        sum += (unsigned int)message[i];
    }
    return sum;
}

int main(int argc, char **argv)
{
    int rank, size, elements, batch_size;
    int *message;

    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));
    lab_require_size(size, 2, 2, rank);
    lab_usage_if(argc != 3, rank,
                 "recepcion_anticipada <elementos> <operaciones_por_test>");
    elements = lab_positive_int(argv[1], "elementos", rank);
    batch_size = lab_positive_int(argv[2], "operaciones_por_test", rank);
    message = lab_alloc_ints(elements, rank);

    if (rank == 0) {
        fill_message(message, elements);
    }
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));

    if (rank == 0) {
        MPI_Request request;
        double start = MPI_Wtime();
        double send_time;

        LAB_MPI(MPI_Isend(message, elements, MPI_INT, 1, DATA_TAG,
                          MPI_COMM_WORLD, &request));
        /* La espera garantiza que el buffer ya se puede liberar. */
        LAB_MPI(MPI_Wait(&request, MPI_STATUS_IGNORE));
        send_time = MPI_Wtime() - start;
        LAB_MPI(MPI_Send(&send_time, 1, MPI_DOUBLE, 1, TIME_TAG,
                         MPI_COMM_WORLD));
    } else {
        MPI_Request request;
        int finished = 0;
        unsigned long long operations = 0;
        unsigned long long checksum;
        uint32_t work_state = 2166136261u;
        double start = MPI_Wtime();
        double receive_time, send_time;

        LAB_MPI(MPI_Irecv(message, elements, MPI_INT, 0, DATA_TAG,
                          MPI_COMM_WORLD, &request));
        /* Un lote se ejecuta antes de cada consulta para mostrar que rank 1
           hace trabajo útil mientras la recepción sigue pendiente. */
        do {
            for (int i = 0; i < batch_size; ++i) {
                work_state = work_state * 1664525u + 1013904223u +
                             (uint32_t)i;
            }
            operations += (unsigned int)batch_size;
            LAB_MPI(MPI_Test(&request, &finished, MPI_STATUS_IGNORE));
        } while (!finished);

        receive_time = MPI_Wtime() - start;
        checksum = verify_and_sum(message, elements);
        LAB_MPI(MPI_Recv(&send_time, 1, MPI_DOUBLE, 0, TIME_TAG,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE));

        printf("Recepción anticipada correcta | mensaje=%zu bytes\n",
               (size_t)elements * sizeof(int));
        printf("Primer entero=%d | suma=%llu | trabajo=%llu operaciones\n",
               message[0], checksum, operations);
        printf("Estado aritmético=%u | tiempo recepción+trabajo=%.9f s\n",
               work_state, receive_time);
        puts("tipo,elementos,bytes,operaciones_por_test,operaciones_total,"
             "tiempo_recepcion_s,tiempo_envio_s,suma");
        printf("recepcion_anticipada,%d,%zu,%d,%llu,%.9f,%.9f,%llu\n",
               elements, (size_t)elements * sizeof(int), batch_size,
               operations, receive_time, send_time, checksum);
    }

    free(message);
    LAB_MPI(MPI_Finalize());
    return EXIT_SUCCESS;
}
