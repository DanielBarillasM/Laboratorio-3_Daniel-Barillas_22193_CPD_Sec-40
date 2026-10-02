/*
 * Laboratorio 3 - presentación visual compartida por los cuatro ejercicios.
 *
 * Estas rutinas solo imprimen DESPUÉS de la región cronometrada. Así, las
 * figuras y colores hacen legibles las capturas sin distorsionar el tiempo
 * de comunicación. Si NO_COLOR tiene algún valor, se omiten los códigos ANSI.
 */
#ifndef LAB3_VISUAL_H
#define LAB3_VISUAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Los códigos ANSI seleccionan colores de la mayoría de terminales WSL. */
#define LAB_ANSI_AZUL    "\x1b[38;5;39m"
#define LAB_ANSI_CIAN    "\x1b[38;5;45m"
#define LAB_ANSI_VERDE   "\x1b[38;5;42m"
#define LAB_ANSI_AMARILLO "\x1b[38;5;220m"
#define LAB_ANSI_GRIS    "\x1b[38;5;245m"
#define LAB_ANSI_RESET   "\x1b[0m"

/* El convenio NO_COLOR facilita guardar registros sin caracteres de escape. */
static inline const char *lab_color(const char *code)
{
    const char *no_color = getenv("NO_COLOR");
    const char *terminal = getenv("TERM");

    if ((no_color != NULL && no_color[0] != '\0') ||
        (terminal != NULL && strcmp(terminal, "dumb") == 0)) {
        return "";
    }
    return code;
}

/* Encabezado común: número del ejercicio, título y familia de comunicación. */
static inline void lab_banner(const char *number, const char *title,
                              const char *family)
{
    char label[64];

    snprintf(label, sizeof(label), "LABORATORIO 03  /  EJERCICIO %s", number);
    printf("\n%s", lab_color(LAB_ANSI_AZUL));
    puts("╔══════════════════════════════════════════════════════════════╗");
    printf("║  %-58s  ║\n", label);
    puts("╚══════════════════════════════════════════════════════════════╝");
    printf("%s%s%s\n", lab_color(LAB_ANSI_CIAN), title,
           lab_color(LAB_ANSI_RESET));
    printf("%s%s%s\n\n", lab_color(LAB_ANSI_GRIS), family,
           lab_color(LAB_ANSI_RESET));
}

/* Sección breve para separar configuración, ruta y medición. */
static inline void lab_section(const char *title)
{
    printf("%s  ── %s ─────────────────────────────────────────────%s\n",
           lab_color(LAB_ANSI_AZUL), title, lab_color(LAB_ANSI_RESET));
}

/* Mensaje de verificación que no se confunde con una fila de datos CSV. */
static inline void lab_success(const char *message)
{
    printf("%s  ✓ %s%s\n", lab_color(LAB_ANSI_VERDE), message,
           lab_color(LAB_ANSI_RESET));
}

/* Barra de veinte celdas usada solo como figura explicativa, no como dato. */
static inline void lab_bar(const char *label, int filled)
{
    if (filled < 0) {
        filled = 0;
    }
    if (filled > 20) {
        filled = 20;
    }
    printf("  %-18s %s[", label, lab_color(LAB_ANSI_CIAN));
    for (int i = 0; i < 20; ++i) {
        putchar(i < filled ? '#' : '.');
    }
    printf("]%s\n", lab_color(LAB_ANSI_RESET));
}

#endif /* LAB3_VISUAL_H */
