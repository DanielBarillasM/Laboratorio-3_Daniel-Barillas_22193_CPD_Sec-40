/*
 * Laboratorio 3 - utilidades compartidas.
 *
 * Este encabezado mantiene los ejemplos independientes de bibliotecas
 * externas. Cada programa se compila directamente con mpicc.
 */
#ifndef LAB3_COMMON_H
#define LAB3_COMMON_H

#include <errno.h>   /* errno señala desbordamiento de strtol. */
#include <limits.h>  /* INT_MAX limita contadores MPI de tipo int. */
#include <stdint.h>  /* SIZE_MAX permite comprobar tamaños de memoria. */
#include <stdio.h>   /* Diagnósticos legibles en stderr. */
#include <stdlib.h>  /* strtol, malloc, exit y códigos de salida. */

#include <mpi.h>

/* Informar el error MPI junto con el lugar donde ocurrió. */
static void lab_mpi_error(int code, const char *expression, const char *file,
                          int line)
{
    char description[MPI_MAX_ERROR_STRING];
    int length = 0;
    int rank = -1;

    /* Identificar el proceso que causó el error. */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* Convertir el código numérico de MPI en texto humano. */
    MPI_Error_string(code, description, &length);
    fprintf(stderr, "[rank %d] Error MPI en %s:%d (%s): %.*s\n", rank,
            file, line, expression, length, description);
    /* Abort termina todos los ranks; un solo proceso no puede seguir solo. */
    MPI_Abort(MPI_COMM_WORLD, code);
}

/* Una macro conserva la expresión y la línea exacta que produjo el fallo. */
#define LAB_MPI(call)                                                       \
    do {                                                                    \
        int lab_mpi_code = (call);                                          \
        if (lab_mpi_code != MPI_SUCCESS) {                                  \
            lab_mpi_error(lab_mpi_code, #call, __FILE__, __LINE__);         \
        }                                                                   \
    } while (0)

/* Todos los ranks reciben los mismos argumentos de mpirun. */
static void lab_usage_if(int invalid, int rank, const char *usage)
{
    if (!invalid) {
        return;
    }
    if (rank == 0) {
        fprintf(stderr, "Uso: %s\n", usage);
    }
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

static int lab_positive_int(const char *text, const char *name, int rank)
{
    char *end = NULL;
    long value;

    /* Limpiar errno antes de convertir para detectar desbordamientos reales. */
    errno = 0;
    value = strtol(text, &end, 10);

    /* end==text: no había dígitos; *end!='\0': sobraba texto. */
    if (errno != 0 || end == text || *end != '\0' || value <= 0 ||
        value > INT_MAX) {
        if (rank == 0) {
            fprintf(stderr, "%s debe ser un entero positivo <= %d.\n",
                    name, INT_MAX);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    return (int)value;
}

static void lab_require_size(int actual, int minimum, int maximum, int rank)
{
    if (actual >= minimum && (maximum < 0 || actual <= maximum)) {
        return;
    }
    if (rank == 0) {
        if (minimum == maximum) {
            fprintf(stderr, "Este ejercicio requiere exactamente %d procesos.\n",
                    minimum);
        } else {
            fprintf(stderr, "Este ejercicio requiere al menos %d procesos.\n",
                    minimum);
        }
    }
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

static int *lab_alloc_ints(int count, int rank)
{
    int *buffer;

    if ((size_t)count > SIZE_MAX / sizeof(*buffer)) {
        if (rank == 0) {
            fputs("El tamaño solicitado desborda la memoria direccionable.\n",
                  stderr);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    /* La reserva real se hace solo después de verificar el producto. */
    buffer = malloc((size_t)count * sizeof(*buffer));
    if (buffer == NULL) {
        fprintf(stderr, "[rank %d] No se pudieron reservar %zu bytes.\n",
                rank, (size_t)count * sizeof(*buffer));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    return buffer;
}

/* MPI ya no puede usarse si Finalize falla: informar sin llamadas MPI extra. */
static void lab_finalize(void)
{
    if (MPI_Finalize() != MPI_SUCCESS) {
        fputs("No se pudo finalizar MPI correctamente.\n", stderr);
        exit(EXIT_FAILURE);
    }
}

#endif /* LAB3_COMMON_H */
