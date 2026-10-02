/*
 * Parte 2, ejercicio 2: productor-consumidor por bloques iguales.
 *
 * Rank 0 llena un arreglo y mantiene hasta dos envíos MPI_Isend pendientes.
 * Rank 1 detecta cada mensaje con MPI_Probe, recibe el chunk completo y lo
 * procesa antes de atender el siguiente. Rank 0 envía finalmente un mensaje
 * STOP con MPI_Isend; rank 1 sale del bucle al recibirlo. El tiempo total
 * incluye comunicación y procesamiento, y ambos ranks verifican la suma.
 *
 * Ejecutar: mpirun -np 2 ./build/pipeline_chunks 1048576 4096
 * Argumentos: <elementos_totales> <elementos_por_chunk>
 * Condición: elementos_totales debe ser divisible por elementos_por_chunk.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

enum { CHUNK_TAG = 41, STOP_TAG = 42, RESULT_TAG = 43, WINDOW = 2 };

static int value_at(int index)
{
    return index % 1000;
}

static unsigned long long fill_input(int *input, int total_elements)
{
    unsigned long long expected_sum = 0;

    for (int i = 0; i < total_elements; ++i) {
        input[i] = value_at(i);
        expected_sum += (unsigned int)input[i];
    }
    return expected_sum;
}

static void produce(const int *input, int total_chunks, int chunk_elements)
{
    MPI_Request pending[WINDOW];

    for (int slot = 0; slot < WINDOW; ++slot) {
        pending[slot] = MPI_REQUEST_NULL;
    }
    for (int chunk = 0; chunk < total_chunks; ++chunk) {
        const int slot = chunk % WINDOW;

        /* Esperar antes de reutilizar el identificador de la petición.
           El arreglo de entrada permanece intacto hasta que todas acaben. */
        if (pending[slot] != MPI_REQUEST_NULL) {
            LAB_MPI(MPI_Wait(&pending[slot], MPI_STATUS_IGNORE));
        }
        LAB_MPI(MPI_Isend(input + (size_t)chunk * chunk_elements,
                          chunk_elements, MPI_INT, 1, CHUNK_TAG,
                          MPI_COMM_WORLD, &pending[slot]));
    }
    LAB_MPI(MPI_Waitall(WINDOW, pending, MPI_STATUSES_IGNORE));

    /* STOP es un mensaje de control de longitud cero, independiente del
       contenido de los chunks. Isend satisface el protocolo solicitado. */
    MPI_Request stop_request;
    LAB_MPI(MPI_Isend(NULL, 0, MPI_BYTE, 1, STOP_TAG, MPI_COMM_WORLD,
                      &stop_request));
    LAB_MPI(MPI_Wait(&stop_request, MPI_STATUS_IGNORE));
}

static void consume(int *chunk_buffer, int chunk_elements,
                    unsigned long long result[2])
{
    result[0] = 0; /* Chunks completados. */
    result[1] = 0; /* Suma de los enteros procesados. */

    for (;;) {
        MPI_Status status;
        int received_elements;

        LAB_MPI(MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status));
        if (status.MPI_TAG == STOP_TAG) {
            LAB_MPI(MPI_Recv(NULL, 0, MPI_BYTE, 0, STOP_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
            break;
        }
        if (status.MPI_TAG != CHUNK_TAG) {
            fputs("Pipeline: etiqueta inesperada.\n", stderr);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        LAB_MPI(MPI_Get_count(&status, MPI_INT, &received_elements));
        if (received_elements != chunk_elements) {
            fprintf(stderr, "Pipeline: chunk de %d enteros; esperado %d.\n",
                    received_elements, chunk_elements);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        LAB_MPI(MPI_Recv(chunk_buffer, chunk_elements, MPI_INT, 0,
                         CHUNK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE));

        /* El consumidor procesa el bloque en cuanto termina su recepción. */
        for (int i = 0; i < chunk_elements; ++i) {
            const size_t global_index =
                (size_t)result[0] * chunk_elements + (size_t)i;
            if (chunk_buffer[i] != value_at((int)global_index)) {
                fprintf(stderr, "Pipeline: valor incorrecto en %zu.\n",
                        global_index);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
            result[1] += (unsigned int)chunk_buffer[i];
        }
        ++result[0];
    }
    LAB_MPI(MPI_Send(result, 2, MPI_UNSIGNED_LONG_LONG, 0, RESULT_TAG,
                     MPI_COMM_WORLD));
}

int main(int argc, char **argv)
{
    int rank, size, total_elements, chunk_elements, total_chunks;
    int *buffer;
    unsigned long long expected_sum = 0;
    unsigned long long result[2] = {0, 0};
    double start, elapsed;

    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));
    lab_require_size(size, 2, 2, rank);
    lab_usage_if(argc != 3, rank,
                 "pipeline_chunks <elementos_totales> <elementos_por_chunk>");
    total_elements = lab_positive_int(argv[1], "elementos_totales", rank);
    chunk_elements = lab_positive_int(argv[2], "elementos_por_chunk", rank);
    if (chunk_elements > total_elements ||
        total_elements % chunk_elements != 0) {
        if (rank == 0) {
            fputs("El tamaño de chunk debe dividir el total exactamente.\n",
                  stderr);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    total_chunks = total_elements / chunk_elements;
    buffer = lab_alloc_ints(rank == 0 ? total_elements : chunk_elements,
                            rank);
    if (rank == 0) {
        expected_sum = fill_input(buffer, total_elements);
    }

    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    start = MPI_Wtime();
    if (rank == 0) {
        produce(buffer, total_chunks, chunk_elements);
        LAB_MPI(MPI_Recv(result, 2, MPI_UNSIGNED_LONG_LONG, 1,
                         RESULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
    } else {
        consume(buffer, chunk_elements, result);
    }
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    elapsed = MPI_Wtime() - start;

    if (rank == 0) {
        if (result[0] != (unsigned long long)total_chunks ||
            result[1] != expected_sum) {
            fprintf(stderr, "Pipeline: esperado %d chunks / suma %llu; "
                    "recibido %llu chunks / suma %llu.\n", total_chunks,
                    expected_sum, result[0], result[1]);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        printf("Pipeline correcto | elementos=%d | chunks=%d | "
               "chunk=%d enteros\n", total_elements, total_chunks,
               chunk_elements);
        printf("STOP recibido | suma verificada=%llu | total=%.9f s\n",
               result[1], elapsed);
        puts("tipo,elementos_totales,elementos_chunk,bytes_chunk,"
             "numero_chunks,tiempo_total_s,suma");
        printf("pipeline_chunks,%d,%d,%zu,%d,%.9f,%llu\n", total_elements,
               chunk_elements, (size_t)chunk_elements * sizeof(int),
               total_chunks, elapsed, result[1]);
    }

    free(buffer);
    LAB_MPI(MPI_Finalize());
    return EXIT_SUCCESS;
}
