/*
 * UNIVERSIDAD DEL VALLE DE GUATEMALA - LABORATORIO 3
 * Ejercicio 2: Token Ring con MPI_Send/MPI_Recv y MPI_Sendrecv.
 *
 * Requisitos del enunciado:
 *   - Usar más de cuatro procesos: el mínimo admitido es cinco.
 *   - Rank i recibe del anterior (i-1+size)%size y envía al siguiente
 *     (i+1)%size.
 *   - Implementar el anillo y ajustarlo para utilizar MPI_Sendrecv.
 *   - Explicar la diferencia entre ambas variantes.
 *
 * Uso: mpirun -np 5 ./build/token_ring <send_recv|sendrecv> <vueltas>
 *
 * Solo existe UN token válido. El entero -1 indica que un rank aún no lo
 * posee. En el modo sendrecv todos los ranks envían en cada paso, pero los
 * ranks sin token solo transfieren ese centinela.
 */
#include <limits.h>  /* INT_MAX, usado para evitar desbordar el token. */
#include <stdio.h>   /* Mensajes y resumen visual. */
#include <stdlib.h>  /* EXIT_SUCCESS, free. */
#include <string.h>  /* strcmp para seleccionar la variante. */

#include "common.h" /* Control de errores y lectura de argumentos. */
#include "visual.h" /* Diagramas, color y barras de visita. */

/* La etiqueta identifica exclusivamente los mensajes del token. */
enum { TOKEN_TAG = 21, EMPTY_TOKEN = -1 };

/*
 * Variante bloqueante con instrucciones separadas.
 * Rank 0 envía primero para iniciar la primera vuelta. Cada otro rank
 * recibe, incrementa el token y luego lo envía. Rank 0 cierra la vuelta
 * al recibir del último. El orden evita un bloqueo circular de envíos.
 */
static void run_send_recv(int rank, int size, int laps, int *token,
                          int *visits)
{
    /* La suma de size hace que el módulo nunca opere sobre un negativo. */
    const int previous = (rank - 1 + size) % size;
    const int next = (rank + 1) % size;

    /* Una vuelta completa lleva el token de rank 0 hasta rank 0. */
    for (int lap = 0; lap < laps; ++lap) {
        if (rank == 0) {
            /* Iniciar la cadena; el buffer no se reutiliza hasta el retorno. */
            LAB_MPI(MPI_Send(token, 1, MPI_INT, next, TOKEN_TAG,
                             MPI_COMM_WORLD));

            /* Esperar el token tras visitar los demás ranks. */
            LAB_MPI(MPI_Recv(token, 1, MPI_INT, previous, TOKEN_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        } else {
            /* Los ranks intermedios esperan a su vecino anterior. */
            LAB_MPI(MPI_Recv(token, 1, MPI_INT, previous, TOKEN_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        }

        /* Cada recepción válida representa una visita del token. */
        ++*token;
        ++*visits;

        if (rank != 0) {
            /* Un rank intermedio pasa el valor incrementado al siguiente. */
            LAB_MPI(MPI_Send(token, 1, MPI_INT, next, TOKEN_TAG,
                             MPI_COMM_WORLD));
        }
    }
}

/*
 * Variante de desplazamiento simultáneo con MPI_Sendrecv.
 * Cada llamada envía el estado local al siguiente rank y recibe el del
 * anterior. Los buffers `token` e `incoming` son diferentes, como exige
 * MPI_Sendrecv. Tras size pasos el token completa una vuelta.
 */
static void run_sendrecv(int rank, int size, int laps, int *token,
                          int *visits)
{
    const int previous = (rank - 1 + size) % size;
    const int next = (rank + 1) % size;
    const int total_steps = size * laps;

    for (int step = 0; step < total_steps; ++step) {
        /* El centinela significa que todavía no llegó el token válido. */
        int incoming = EMPTY_TOKEN;

        /* MPI coordina el envío y la recepción en una llamada bloqueante. */
        LAB_MPI(MPI_Sendrecv(token, 1, MPI_INT, next, TOKEN_TAG,
                             &incoming, 1, MPI_INT, previous, TOKEN_TAG,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE));

        /* El estado local del siguiente paso es lo que acaba de recibirse. */
        *token = incoming;

        if (incoming != EMPTY_TOKEN) {
            /* Solo el poseedor del token suma una visita y una unidad. */
            ++*token;
            ++*visits;
        }
    }
}

/* Confirmar las invariantes del anillo en rank 0. */
static void verify_ring(const int *all_visits, int size, int laps,
                        int final_token)
{
    /* Cada rank incrementa una vez por vuelta: valor final = size*laps. */
    if (final_token != size * laps) {
        fprintf(stderr, "Token final incorrecto: %d; esperado: %d.\n",
                final_token, size * laps);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Una visita perdida o duplicada invalida la simulación del anillo. */
    for (int i = 0; i < size; ++i) {
        if (all_visits[i] != laps) {
            fprintf(stderr, "Rank %d recibió %d veces; esperado: %d.\n",
                    i, all_visits[i], laps);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }
}

/* La figura ayuda a leer la ruta sin depender del orden de printf por rank. */
static void print_route(int size)
{
    printf("%s  ", lab_color(LAB_ANSI_CIAN));
    for (int i = 0; i < size; ++i) {
        printf("[%d] ──▶ ", i);
        /* Un salto de línea evita una fila demasiado larga con muchos ranks. */
        if ((i + 1) % 8 == 0 && i + 1 < size) {
            printf("\n  ");
        }
    }
    printf("[0]%s\n", lab_color(LAB_ANSI_RESET));
}

/* Imprimir comprobaciones y una fila CSV después de la región cronometrada. */
static void print_summary(const char *mode, int size, int laps,
                          int final_token, const int *all_visits,
                          double elapsed)
{
    lab_banner("02", "TOKEN RING", "Comunicación bloqueante | más de 4 procesos");
    lab_section("Topología del anillo");
    print_route(size);
    puts("  src(i) = (i - 1 + size) % size");
    puts("  dst(i) = (i + 1) % size");

    lab_section("Configuración");
    printf("  Modo                : %s\n", mode);
    printf("  Procesos            : %d\n", size);
    printf("  Vueltas             : %d\n", laps);
    printf("  Token inicial       : 0 en rank 0\n");

    lab_section("Visitas del token");
    for (int i = 0; i < size; ++i) {
        char label[32];
        snprintf(label, sizeof(label), "Rank %d", i);
        lab_bar(label, 20);
        printf("    %d / %d visitas\n", all_visits[i], laps);
    }
    lab_success("Cada rank recibió el token exactamente una vez por vuelta.");

    lab_section("Resultado y tiempo");
    printf("  Token final         : %d (esperado: %d)\n",
           final_token, size * laps);
    printf("  Tiempo en rank 0    : %.9f s\n", elapsed);
    printf("%s  %s%s\n\n", lab_color(LAB_ANSI_AMARILLO),
           strcmp(mode, "sendrecv") == 0
               ? "Sendrecv coordina envío y recepción en una llamada."
               : "Send y Recv requieren un orden seguro entre ranks.",
           lab_color(LAB_ANSI_RESET));

    /* Esta cabecera mantiene el formato esperado por mediciones.sh. */
    puts("tipo,modo,procesos,vueltas,token_final,tiempo_rank0_s");
    printf("token_ring,%s,%d,%d,%d,%.9f\n", mode, size, laps,
           final_token, elapsed);
}

int main(int argc, char **argv)
{
    int rank, size;
    int laps;
    int token;
    int visits = 0;
    int *all_visits = NULL;
    int use_sendrecv;
    double start, elapsed;

    /* Crear el comunicador MPI antes de consultar ranks o usar la red. */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo iniciar MPI.\n", stderr);
        return EXIT_FAILURE;
    }
    LAB_MPI(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
    LAB_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    LAB_MPI(MPI_Comm_size(MPI_COMM_WORLD, &size));

    /* `-np mayor a 4` significa al menos cinco procesos. */
    lab_require_size(size, 5, -1, rank);
    lab_usage_if(argc != 3, rank,
                 "token_ring <sendrecv|send_recv> <vueltas>");

    /* Elegir exactamente una de las dos variantes documentadas. */
    use_sendrecv = strcmp(argv[1], "sendrecv") == 0;
    lab_usage_if(!use_sendrecv && strcmp(argv[1], "send_recv") != 0, rank,
                 "token_ring <sendrecv|send_recv> <vueltas>");

    /* Las vueltas deben ser positivas y caber en el contador final. */
    laps = lab_positive_int(argv[2], "vueltas", rank);
    if (laps > INT_MAX / size) {
        if (rank == 0) {
            fputs("Demasiadas vueltas para el contador del token.\n", stderr);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Solo rank 0 empieza con el token; los demás empiezan vacíos. */
    token = rank == 0 ? 0 : EMPTY_TOKEN;

    /* MPI_Gather almacenará una cuenta de visitas por rank en el origen. */
    if (rank == 0) {
        all_visits = lab_alloc_ints(size, rank);
    }

    /* Sincronizar antes de iniciar la medición local de rank 0. */
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    start = MPI_Wtime();

    /* Ejecutar exactamente la variante solicitada en todos los procesos. */
    if (use_sendrecv) {
        run_sendrecv(rank, size, laps, &token, &visits);
    } else {
        run_send_recv(rank, size, laps, &token, &visits);
    }

    /* La barrera final incluye la terminación del rank más lento. */
    LAB_MPI(MPI_Barrier(MPI_COMM_WORLD));
    elapsed = MPI_Wtime() - start;

    /* Reunir las visitas una vez terminado el tiempo cronometrado. */
    LAB_MPI(MPI_Gather(&visits, 1, MPI_INT, all_visits, 1, MPI_INT, 0,
                       MPI_COMM_WORLD));

    if (rank == 0) {
        /* La comprobación de corrección precede a cualquier reporte. */
        verify_ring(all_visits, size, laps, token);
        print_summary(argv[1], size, laps, token, all_visits, elapsed);
    }

    /* Rank 0 libera el arreglo; en otros ranks free(NULL) es seguro. */
    free(all_visits);
    lab_finalize();
    return EXIT_SUCCESS;
}
