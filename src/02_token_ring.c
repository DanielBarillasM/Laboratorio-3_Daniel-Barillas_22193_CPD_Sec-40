/*
 * Parte 1, ejercicio 2: Token Ring con más de cuatro procesos.
 *
 * Hay un único token válido. En modo sendrecv, los demás ranks envían
 * el valor centinela -1; una llamada MPI_Sendrecv por paso mueve el token
 * un salto y evita el bloqueo circular. En modo send_recv, rank 0 inicia
 * la cadena y cada otro rank recibe antes de enviar. Ambas variantes
 * producen el mismo número de visitas para poder compararlas.
 *
 * Ejecutar: mpirun -np 5 ./build/token_ring sendrecv 3
 *           mpirun -np 5 ./build/token_ring send_recv 3
 * Argumentos: <sendrecv|send_recv> <vueltas>
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

enum { TOKEN_TAG = 21, EMPTY_TOKEN = -1 };

static void run_send_recv(int rank, int size, int laps, int *token,
                          int *visits)
{
    const int previous = (rank - 1 + size) % size;
    const int next = (rank + 1) % size;

    for (int lap = 0; lap < laps; ++lap) {
        if (rank == 0) {
            /* El rank 0 debe enviar primero para poner el token en marcha. */
            LAB_MPI(MPI_Send(token, 1, MPI_INT, next, TOKEN_TAG,
                             MPI_COMM_WORLD));
            LAB_MPI(MPI_Recv(token, 1, MPI_INT, previous, TOKEN_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        } else {
            LAB_MPI(MPI_Recv(token, 1, MPI_INT, previous, TOKEN_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        }

        /* La suma demuestra que el token atravesó exactamente un rank. */
        ++*token;
        ++*visits;
        if (rank != 0) {
            LAB_MPI(MPI_Send(token, 1, MPI_INT, next, TOKEN_TAG,
                             MPI_COMM_WORLD));
        }
    }
}

static void run_sendrecv(int rank, int size, int laps, int *token,
                          int *visits)
{
    const int previous = (rank - 1 + size) % size;
    const int next = (rank + 1) % size;
    const int total_steps = size * laps;

    for (int step = 0; step < total_steps; ++step) {
        int incoming = EMPTY_TOKEN;

        /* Todos los ranks envían y reciben en la misma llamada. El token
           válido solo existe en uno de ellos en cada paso. */
        LAB_MPI(MPI_Sendrecv(token, 1, MPI_INT, next, TOKEN_TAG,
                             &incoming, 1, MPI_INT, previous, TOKEN_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        *token = incoming;
        if (incoming != EMPTY_TOKEN) {
            ++*token;
            ++*visits;
        }
    }
}

int main(int argc, char **argv)
{
    int rank, size, laps, token, visits = 0;
    int *all_visits = NULL;
    int use_sendrecv;
    double start, elapsed;

    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));
    lab_require_size(size, 5, -1, rank);
    lab_usage_if(argc != 3, rank,
                 "token_ring <sendrecv|send_recv> <vueltas>");
    use_sendrecv = strcmp(argv[1], "sendrecv") == 0;
    lab_usage_if(!use_sendrecv && strcmp(argv[1], "send_recv") != 0, rank,
                 "token_ring <sendrecv|send_recv> <vueltas>");
    laps = lab_positive_int(argv[2], "vueltas", rank);
    if (laps > INT_MAX / size) {
        if (rank == 0) {
            fputs("Demasiadas vueltas para el contador del token.\n", stderr);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    token = rank == 0 ? 0 : EMPTY_TOKEN;
    if (rank == 0) {
        all_visits = lab_alloc_ints(size, rank);
    }
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    start = MPI_Wtime();

    if (use_sendrecv) {
        run_sendrecv(rank, size, laps, &token, &visits);
    } else {
        run_send_recv(rank, size, laps, &token, &visits);
    }

    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    elapsed = MPI_Wtime() - start;
    LAB_MPI(MPI_Gather(&visits, 1, MPI_INT, all_visits, 1, MPI_INT, 0,
                       MPI_COMM_WORLD));

    if (rank == 0) {
        if (token != size * laps) {
            fprintf(stderr, "Token final incorrecto: %d; esperado: %d.\n",
                    token, size * laps);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        for (int i = 0; i < size; ++i) {
            if (all_visits[i] != laps) {
                fprintf(stderr, "Rank %d recibió %d veces; esperado: %d.\n",
                        i, all_visits[i], laps);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
        printf("Token Ring correcto | modo=%s | procesos=%d | vueltas=%d\n",
               argv[1], size, laps);
        printf("Ruta: ");
        for (int i = 0; i < size; ++i) {
            printf("%d -> ", i);
        }
        puts("0");
        printf("Token final=%d | tiempo local rank 0=%.9f s\n",
               token, elapsed);
        for (int i = 0; i < size; ++i) {
            printf("Rank %d: %d visitas\n", i, all_visits[i]);
        }
        puts("tipo,modo,procesos,vueltas,token_final,tiempo_rank0_s");
        printf("token_ring,%s,%d,%d,%d,%.9f\n", argv[1], size, laps,
               token, elapsed);
    }

    free(all_visits);
    LAB_MPI(MPI_Finalize());
    return EXIT_SUCCESS;
}
