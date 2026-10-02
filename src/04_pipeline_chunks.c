/*
 * UNIVERSIDAD DEL VALLE DE GUATEMALA - LABORATORIO 3
 * Ejercicio 4: productor-consumidor y pipeline por chunks iguales.
 *
 * Requisitos del enunciado:
 *   - Ejecutar siempre con dos procesos.
 *   - Rank 0 crea un arreglo grande y lo divide en chunks iguales.
 *   - Rank 1 procesa cada chunk en cuanto termina de recibirlo.
 *   - Rank 0 envía una señal STOP con MPI_Isend al finalizar.
 *   - Medir tiempo total para varios tamaños de chunk.
 *
 * Uso: mpirun -np 2 ./build/pipeline_chunks <total> <chunk>
 * Ejemplo: mpirun -np 2 ./build/pipeline_chunks 1048576 4096
 *
 * El total debe ser divisible exactamente por el tamaño de chunk.
 * Se usan dos peticiones de envío para mantener hasta dos chunks en vuelo.
 * Los dibujos de consola se generan después de MPI_Wtime, no dentro de la
 * región medida.
 */
#include <stdio.h>   /* Presentación y diagnóstico. */
#include <stdlib.h>  /* free y códigos de salida. */

#include "common.h" /* MPI, argumentos y reserva segura. */
#include "visual.h" /* Paneles y figura de los chunks. */

/* Tres etiquetas separan datos, control STOP y confirmación final. */
enum { CHUNK_TAG = 41, STOP_TAG = 42, RESULT_TAG = 43, WINDOW = 2 };

/* Patrón conocido que permite verificar cada posición del arreglo. */
static int value_at(int index)
{
    return index % 1000;
}

/* Rank 0 prepara los datos y calcula la suma esperada. */
static unsigned long long fill_input(int *input, int total_elements)
{
    unsigned long long expected_sum = 0;

    for (int i = 0; i < total_elements; ++i) {
        input[i] = value_at(i);
        expected_sum += (unsigned int)input[i];
    }
    return expected_sum;
}

/*
 * Rank 0: publicar dos envíos no bloqueantes como máximo.
 * pending[slot] conserva la petición del chunk anterior que ocupó el slot.
 * El buffer de datos no se altera hasta que MPI_Waitall ha terminado.
 */
static void produce(const int *input, int total_chunks, int chunk_elements)
{
    MPI_Request pending[WINDOW];

    /* MPI_REQUEST_NULL permite esperar slots aún no utilizados. */
    for (int slot = 0; slot < WINDOW; ++slot) {
        pending[slot] = MPI_REQUEST_NULL;
    }

    for (int chunk = 0; chunk < total_chunks; ++chunk) {
        /* Alternar 0,1,0,1 produce una ventana de dos transferencias. */
        const int slot = chunk % WINDOW;

        if (pending[slot] != MPI_REQUEST_NULL) {
            /* Antes de reutilizar el slot, terminar su envío anterior. */
            LAB_MPI(MPI_Wait(&pending[slot], MPI_STATUS_IGNORE));
        }

        /* Desplazar el puntero al comienzo exacto del chunk actual. */
        const int *chunk_start = input + (size_t)chunk * chunk_elements;

        /* Isend inicia el envío y deja una petición para completarlo luego. */
        LAB_MPI(MPI_Isend(chunk_start, chunk_elements, MPI_INT, 1,
                          CHUNK_TAG, MPI_COMM_WORLD, &pending[slot]));
    }

    /* Completar ambos slots antes de emitir la señal de fin. */
    LAB_MPI(MPI_Waitall(WINDOW, pending, MPI_STATUSES_IGNORE));

    /* STOP no transporta enteros: su etiqueta comunica el final del flujo. */
    MPI_Request stop_request;
    LAB_MPI(MPI_Isend(NULL, 0, MPI_BYTE, 1, STOP_TAG, MPI_COMM_WORLD,
                      &stop_request));

    /* Confirmar STOP antes de salir de la función del productor. */
    LAB_MPI(MPI_Wait(&stop_request, MPI_STATUS_IGNORE));
}

/*
 * Rank 1: esperar un mensaje, identificarlo por etiqueta y procesarlo.
 * result[0] es la cantidad de chunks; result[1] es la suma acumulada.
 * La recepción termina SOLO cuando aparece el mensaje STOP.
 */
static void consume(int *chunk_buffer, int chunk_elements,
                    unsigned long long result[2])
{
    result[0] = 0;
    result[1] = 0;

    for (;;) {
        MPI_Status status;
        int received_elements;

        /* MPI_ANY_TAG permite detectar tanto datos como señal de parada. */
        LAB_MPI(MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status));

        if (status.MPI_TAG == STOP_TAG) {
            /* Consumir el mensaje de control y romper el bucle de trabajo. */
            LAB_MPI(MPI_Recv(NULL, 0, MPI_BYTE, 0, STOP_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
            break;
        }

        /* Cualquier etiqueta diferente de CHUNK o STOP es un error. */
        if (status.MPI_TAG != CHUNK_TAG) {
            fputs("Pipeline: etiqueta inesperada.\n", stderr);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /* Confirmar que el mensaje realmente contiene un chunk entero. */
        LAB_MPI(MPI_Get_count(&status, MPI_INT, &received_elements));
        if (received_elements != chunk_elements) {
            fprintf(stderr, "Pipeline: chunk de %d enteros; esperado %d.\n",
                    received_elements, chunk_elements);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        /* Recibir el chunk dentro del buffer reutilizable de rank 1. */
        LAB_MPI(MPI_Recv(chunk_buffer, chunk_elements, MPI_INT, 0,
                         CHUNK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE));

        /* Procesar AHORA, antes de pedir el siguiente mensaje al productor. */
        for (int i = 0; i < chunk_elements; ++i) {
            /* Convertir índice local del chunk en índice del arreglo total. */
            const size_t global_index =
                (size_t)result[0] * chunk_elements + (size_t)i;

            /* La comparación comprueba tanto contenido como orden. */
            if (chunk_buffer[i] != value_at((int)global_index)) {
                fprintf(stderr, "Pipeline: valor incorrecto en %zu.\n",
                        global_index);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
            result[1] += (unsigned int)chunk_buffer[i];
        }

        /* Registrar el chunk solo después de procesar todos sus enteros. */
        ++result[0];
    }

    /* Informar a rank 0 qué se procesó realmente. */
    LAB_MPI(MPI_Send(result, 2, MPI_UNSIGNED_LONG_LONG, 0, RESULT_TAG,
                     MPI_COMM_WORLD));
}

/* Comprobar al final que no faltó ni sobró un chunk. */
static void verify_result(const unsigned long long result[2],
                          int total_chunks, unsigned long long expected_sum)
{
    if (result[0] != (unsigned long long)total_chunks ||
        result[1] != expected_sum) {
        fprintf(stderr, "Pipeline: esperado %d chunks / suma %llu; "
                "recibido %llu chunks / suma %llu.\n", total_chunks,
                expected_sum, result[0], result[1]);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
}

/* Figura esquemática: se limita a seis bloques para mantenerla legible. */
static void print_chunk_diagram(int total_chunks)
{
    const int visible = total_chunks < 6 ? total_chunks : 6;

    printf("%s  RANK 0  [ARREGLO]  ──▶  ", lab_color(LAB_ANSI_CIAN));
    for (int i = 0; i < visible; ++i) {
        printf("[C%d]", i);
    }
    if (total_chunks > visible) {
        printf(" ... [C%d]", total_chunks - 1);
    }
    printf("  ──▶  RANK 1\n");
    printf("  RANK 0  [STOP, tag %d] ───────────────────▶ [fin del bucle]%s\n",
           STOP_TAG, lab_color(LAB_ANSI_RESET));
    puts("  (Figura ilustrativa; cada C representa un chunk.)");
}

/* Mostrar la figura, las comprobaciones y la fila CSV fuera del cronómetro. */
static void print_summary(int total_elements, int chunk_elements,
                          int total_chunks, unsigned long long sum,
                          double elapsed)
{
    const size_t bytes_per_chunk = (size_t)chunk_elements * sizeof(int);

    lab_banner("04", "PIPELINE POR CHUNKS",
               "Productor-consumidor | 2 procesos | STOP con MPI_Isend");
    lab_section("Flujo de datos");
    print_chunk_diagram(total_chunks);

    lab_section("Configuración");
    printf("  Enteros totales      : %d\n", total_elements);
    printf("  Enteros por chunk    : %d\n", chunk_elements);
    printf("  Bytes por chunk      : %zu\n", bytes_per_chunk);
    printf("  Número de chunks     : %d\n", total_chunks);
    printf("  Ventana de envíos    : %d peticiones MPI_Isend\n", WINDOW);

    lab_section("Verificación del consumidor");
    lab_bar("Chunks procesados", 20);
    lab_success("Todos los chunks llegaron en orden y fueron procesados.");
    lab_success("El mensaje STOP cerró el bucle de rank 1.");
    printf("  Suma comprobada      : %llu\n", sum);

    lab_section("Tiempo total con MPI_Wtime");
    printf("  Comunicación + trabajo: %.9f s\n\n", elapsed);

    puts("tipo,elementos_totales,elementos_chunk,bytes_chunk,"
         "numero_chunks,tiempo_total_s,suma");
    printf("pipeline_chunks,%d,%d,%zu,%d,%.9f,%llu\n", total_elements,
           chunk_elements, bytes_per_chunk, total_chunks, elapsed, sum);
}

int main(int argc, char **argv)
{
    int rank, size, total_elements, chunk_elements, total_chunks;
    int *buffer;
    unsigned long long expected_sum = 0;
    unsigned long long result[2] = {0, 0};
    double start, elapsed;

    /* Crear el entorno MPI en ambos procesos. */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));

    /* El enunciado fija una pareja productor-consumidor. */
    lab_require_size(size, 2, 2, rank);
    lab_usage_if(argc != 3, rank,
                 "pipeline_chunks <elementos_totales> <elementos_por_chunk>");

    /* Ambas cantidades se leen desde la terminal y se validan. */
    total_elements = lab_positive_int(argv[1], "elementos_totales", rank);
    chunk_elements = lab_positive_int(argv[2], "elementos_por_chunk", rank);

    /* Chunks iguales requieren divisibilidad exacta. */
    if (chunk_elements > total_elements ||
        total_elements % chunk_elements != 0) {
        if (rank == 0) {
            fputs("El tamaño de chunk debe dividir el total exactamente.\n",
                  stderr);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    total_chunks = total_elements / chunk_elements;

    /* Rank 0 almacena todo el arreglo; rank 1 solo necesita un chunk. */
    buffer = lab_alloc_ints(rank == 0 ? total_elements : chunk_elements,
                            rank);
    if (rank == 0) {
        expected_sum = fill_input(buffer, total_elements);
    }

    /* Separar la preparación de datos del tiempo de transferencia. */
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    start = MPI_Wtime();

    if (rank == 0) {
        /* Producir todos los chunks y enviar la señal de finalización. */
        produce(buffer, total_chunks, chunk_elements);

        /* El consumidor confirma cuántos chunks procesó y cuál fue su suma. */
        LAB_MPI(MPI_Recv(result, 2, MPI_UNSIGNED_LONG_LONG, 1,
                         RESULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
    } else {
        /* El consumidor no sale hasta recibir STOP. */
        consume(buffer, chunk_elements, result);
    }

    /* La barrera obliga a incluir el fin del trabajo de ambos procesos. */
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    elapsed = MPI_Wtime() - start;

    if (rank == 0) {
        /* Validar y dibujar únicamente después de cerrar la medición. */
        verify_result(result, total_chunks, expected_sum);
        print_summary(total_elements, chunk_elements, total_chunks,
                      result[1], elapsed);
    }

    /* Ya no hay envíos pendientes y la memoria puede liberarse. */
    free(buffer);
    lab_finalize();
    return EXIT_SUCCESS;
}
