# Auditoría de cierre · Laboratorio 3

Fecha: 2 de octubre de 2026. Referencia: `Laboratorio 3.pdf`, proporcionado por
el estudiante. Entrega vigente confirmada: **3 de octubre de 2026, 23:59**.
La fecha de 2025 del enunciado no se adopta como fecha vigente.

## Correspondencia con el enunciado

| Ejercicio e inciso | Implementación | Evidencia y análisis |
|:---|:---|:---|
| Ping-Pong 1: dos ranks, entero, N definido por usuario | `01_ping_pong.c`; `elementos=1`; exactamente N rondas por defecto | `01_ping_pong_entero.png`; apartado del inciso 1 |
| Ping-Pong 2: MPI_Wtime, tamaños, gráfica y tendencia | Tiempo local en rank 0, RTT por N rondas, tamaños parametrizables | `02_ping_pong_tamanos.png`, 15 filas CSV, tabla y gráfica |
| Token Ring 1: más de cuatro ranks y vecinos modulares | `02_token_ring.c`; mínimo cinco procesos; origen/destino según PDF | `03_token_ring_send_recv.png`; token final y visitas |
| Token Ring 2 y 3: Sendrecv y diferencia con Send/Recv | Dos modos; Sendrecv con buffers distintos; orden seguro en modo separado | `04_token_ring_sendrecv.png`, seis filas, diferencias funcionales y tráfico explicado |
| Recepción 1 y 2: entero, aritmética, MPI_Test y salida | `03_recepcion_anticipada.c`; Irecv antes del cálculo; sondeos; estado y contador | `05_recepcion_entero.png`; se distingue petición MPI de aritmética |
| Recepción 3: tiempo según tamaño | Tamaños parametrizables; duración local del receptor y del emisor | `06_recepcion_tamanos.png`, 15 filas, gráfica y discusión |
| Pipeline 1 y 2: dos ranks, arreglo, chunks iguales y STOP con Isend | `04_pipeline_chunks.c`; divisibilidad; consumo inmediato; espera de peticiones y STOP | `07_pipeline_chunks.png`; suma y cantidad de chunks verificadas |
| Pipeline 3: tiempo total frente a chunk | Volumen fijo, cinco tamaños de bloque, MPI_Wtime local | `08_pipeline_tamanos.png`, 15 filas, gráfica y discusión |
| Screenshots antes de los incisos resueltos | Ocho PNG originales; encabezados identifican cada inciso o grupo de incisos | Capturas preceden la explicación de sus apartados en el informe |
| Informe PDF y fuentes; no ejecutables | LaTeX y PDF; cuatro `.c` con sus dos `.h`; Makefile | ZIP con lista explícita de 36 archivos; excluye build, binarios y auxiliares |

## Correcciones de cierre

- El calentamiento de Ping-Pong dejó de ser implícito: sin `--warmup` solo
  ocurren N intercambios. La opción añade una ronda preparatoria no medida.
- `mediciones.sh` usa `--warmup` para conservar el protocolo de las mediciones
  históricas. El formato CSV y las 51 observaciones no se modificaron.
- README e informe explican esta diferencia de versión. Las capturas originales
  no se atribuyen a una ejecución nueva del código corregido.
- Los apartados del informe identifican explícitamente los incisos y presentan
  las capturas antes de su explicación. Se precisa qué completa `MPI_Test`.
- Se corrigió un comentario que nombraba erróneamente la macro `LAB_MPI`.
- Se evitó que LaTeX convierta los dos guiones de opciones de terminal en una
  ligadura; `--warmup` y `--oversubscribe` se conservan al copiar desde el PDF.
- El paquete incluye los encabezados indispensables; no incorpora ejecutables.
- Se conservaron las eliminaciones de los `.gitkeep` de carpetas ya pobladas,
  ya registradas en el commit `76ae861`; no se eliminaron resultados ni capturas.

## Comprobaciones y límites

- Cuatro fuentes compilados con Open MPI, C11 y las advertencias del Makefile.
  Inspección sintáctica adicional con `-Werror -fsyntax-only`.
- Sintaxis de Bash y Python comprobada; analizador ejecutado únicamente sobre
  los CSV existentes. Se mantienen 17 configuraciones y tres repeticiones.
- Tablas y conclusiones mantienen los datos reales ya analizados. La dispersión
  se presenta sin descartar corridas ni afirmar significancia estadística.
- LaTeX recompilado y páginas renderizadas revisadas; rutas de las ocho capturas
  y las cuatro gráficas verificadas. ZIP contrastado con su lista de inclusión.
- No se ejecutaron `mpirun`, los programas MPI ni el barrido durante este cierre.
  Las verificaciones de ejecución se sustentan en las evidencias del estudiante;
  la nueva opción de Ping-Pong se verificó por inspección y compilación, no por
  una nueva prueba distribuida. No se fabricaron ni sustituyeron mediciones.
- La comparación del anillo no aísla primitivas: los modos generan 5000 y
  25 000 mensajes, respectivamente. La recepción incluye cálculo y sondeos;
  el pipeline no tiene referencia secuencial. Los resultados son descriptivos.
- La publicación en GitHub y la generación del ZIP no entregan el trabajo en
  Canvas. Esa acción final corresponde al estudiante.
