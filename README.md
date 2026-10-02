# Laboratorio 3 · Comunicación con MPI

**Computación Paralela y Distribuida · UVG**

**Daniel Barillas · 22193 · Sección 40**

**Entrega:** sábado 3 de octubre de 2026, 23:59 (Guatemala)

Este repositorio reúne cuatro ejercicios del enunciado: Ping-Pong y Token Ring
con comunicación bloqueante; recepción anticipada y productor-consumidor por
chunks con comunicación sin bloqueo. Los programas verifican sus resultados y
emiten una fila CSV para facilitar las gráficas del informe.

> **Estado:** el código y la metodología están preparados. Las mediciones,
> capturas y conclusiones experimentales se completarán con las ejecuciones
> realizadas por el estudiante en WSL. No se inventan resultados.

## Contenido

```text
.
├── src/
│   ├── common.h                     Utilidades de argumentos y errores MPI
│   ├── 01_ping_pong.c               Intercambio bloqueante de ida y vuelta
│   ├── 02_token_ring.c              Anillo: Send/Recv y Sendrecv
│   ├── 03_recepcion_anticipada.c    Irecv + trabajo + Test
│   └── 04_pipeline_chunks.c         Productor-consumidor + STOP con Isend
├── scripts/
│   └── mediciones.sh                Barridos experimentales (ejecutar en WSL)
├── informe/
│   └── informe.tex                  Fuente del informe (y PDF al compilar)
├── evidencias/                     Capturas del estudiante, cuando se reciban
├── resultados/                     CSV tras las ejecuciones del estudiante
├── Makefile
└── README.md
```

El directorio `build/` contiene los ejecutables locales y está excluido de
Git. La entrega a Canvas incluye los archivos fuente y el informe PDF, **sin
ejecutables**.

## Preparar WSL y compilar

Abrir **Ubuntu en WSL** e instalar el compilador y Open MPI si aún faltan:

```bash
sudo apt update
sudo apt install build-essential openmpi-bin libopenmpi-dev
```

Entrar a la carpeta del proyecto y compilar:

```bash
cd "/mnt/c/Users/Daniel Barillas/Desktop/Lab-3_Paralela/Laboratorio-3_Daniel-Barillas_22193_CPD_Sec-40"
make
```

`make` construye cuatro ejecutables dentro de `build/`. Si se desea compilar
uno manualmente, por ejemplo:

```bash
mkdir -p build
mpicc -std=c11 -O2 -Wall -Wextra -Wpedantic src/01_ping_pong.c -o build/ping_pong
```

## Ejecución demostrativa paso a paso

Todos los comandos se ejecutan desde la raíz del repositorio. Las salidas
incluyen texto legible y una última línea CSV. Para capturas, usa una terminal
lo suficientemente ancha para que se vean el comando y el resultado completo.

### 1. Ping-Pong

```bash
mpirun -np 2 ./build/ping_pong 10 1
mpirun -np 2 ./build/ping_pong 1000 1024
```

El primer comando intercambia un único entero diez veces. El segundo mantiene
el mismo protocolo con 1024 enteros por mensaje (4096 bytes). El tiempo
mostrado es la duración total y el promedio de ida y vuelta. La ida estimada
divide este último entre dos y supone latencias aproximadamente simétricas.

### 2. Token Ring

```bash
mpirun --oversubscribe -np 5 ./build/token_ring send_recv 1
mpirun --oversubscribe -np 5 ./build/token_ring sendrecv 1
```

`-np 5` cumple el requisito de más de cuatro procesos. `--oversubscribe`
permite la demostración incluso cuando WSL informa menos de cinco slots; si
hay suficientes, se puede omitir. Ambas variantes deben devolver un token
final de 5 y una visita por rank. El origen y destino de cada rank son
`(i - 1 + size) % size` y `(i + 1) % size`.

### 3. Recepción anticipada

```bash
mpirun -np 2 ./build/recepcion_anticipada 1 1000
mpirun -np 2 ./build/recepcion_anticipada 16384 1000
```

El rank 1 publica `MPI_Irecv`, realiza 1000 operaciones aritméticas por lote y
consulta `MPI_Test` hasta que el mensaje llega. El número total de operaciones
puede variar entre corridas: depende de cuándo se complete la comunicación.

### 4. Pipeline por chunks

```bash
mpirun -np 2 ./build/pipeline_chunks 1048576 4096
mpirun -np 2 ./build/pipeline_chunks 1048576 65536
```

Rank 0 crea un arreglo de 1 048 576 enteros y envía chunks iguales. Rank 1
procesa cada bloque después de recibirlo. El último mensaje es `STOP`, enviado
con `MPI_Isend`. La salida confirma el número de chunks y la suma verificada.

## Mediciones para las gráficas

El script prepara tres repeticiones para cada tamaño de mensaje o chunk,
además de comparar las dos variantes del anillo. **Solo el estudiante debe
ejecutarlo** dentro de WSL:

```bash
bash scripts/mediciones.sh
```

Se generan cuatro archivos `resultados/*.csv` y salidas completas en
`resultados/brutos/`. El script se detiene si ya existen CSV, para no
sobrescribir mediciones previas. Envíame los cuatro CSV y las capturas para
completar las gráficas y el análisis del informe. En las comparaciones, usaré
la mediana de las tres repeticiones y mostraré la dispersión cuando sea útil.

| Archivo | Parámetro que cambia | Magnitud principal |
|---|---|---|
| `ping_pong.csv` | Bytes por mensaje | Ida y vuelta media (µs) |
| `token_ring.csv` | `send_recv` / `sendrecv` | Tiempo de rank 0 (s) |
| `recepcion_anticipada.csv` | Bytes por mensaje | Tiempo de recepción + trabajo (s) |
| `pipeline_chunks.csv` | Bytes por chunk | Tiempo total (s) |

Para evitar comparaciones engañosas, mantén constante el número de procesos,
el tamaño de trabajo aritmético, las rondas y el total de datos según el
ejercicio. Evita ejecutar otras aplicaciones pesadas durante las mediciones.

## Capturas necesarias

Guardar las imágenes en `evidencias/`, preferiblemente PNG, con nombres
descriptivos. Cada captura debe mostrar el comando ejecutado y su resultado:

1. `01_ping_pong_entero.png`: caso de un entero y `N` elegido por ti.
2. `02_ping_pong_tamanos.png`: al menos dos tamaños distintos.
3. `03_token_ring_send_recv.png`: variante con `MPI_Send` y `MPI_Recv`.
4. `04_token_ring_sendrecv.png`: variante con `MPI_Sendrecv`.
5. `05_recepcion_entero.png`: recepción de un entero y trabajo aritmético.
6. `06_recepcion_tamanos.png`: al menos un mensaje grande.
7. `07_pipeline_chunks.png`: procesamiento, `STOP` y suma correcta.
8. `08_pipeline_tamanos.png`: otro tamaño de chunk.

Si una captura de varios tamaños no cabe, usa varias imágenes. El informe
colocará la evidencia al inicio del apartado pertinente, antes del análisis
de sus resultados. También es útil adjuntar el resultado de `mpicc --version`
y `mpirun --version` para documentar el entorno.

## Informe

El archivo `informe/informe.tex` contiene el marco teórico, diseño y método
experimental. Las tablas, gráficas y discusión final se completarán con las
mediciones y capturas reales. El PDF final se compilará y revisará visualmente
antes de la entrega.

## Referencia

Enunciado proporcionado por el curso: *Laboratorio #3*, Computación Paralela
y Distribuida, Universidad del Valle de Guatemala, semestre 2 de 2025. La
fecha de entrega vigente comunicada para esta realización es el 3 de octubre
de 2026 a las 23:59.
