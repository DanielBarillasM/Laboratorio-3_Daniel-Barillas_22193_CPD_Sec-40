#!/usr/bin/env bash
# Ejecutar manualmente desde la raíz del repositorio, dentro de WSL.
# Realiza tres repeticiones por configuración y conserva las salidas crudas.
set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p resultados/brutos
make

# Las demostraciones manuales conservan color. En el barrido se guardan
# registros limpios para que el texto y el CSV sean fáciles de procesar.
export NO_COLOR=1

for table in ping_pong token_ring recepcion_anticipada pipeline_chunks; do
    if [[ -e "resultados/${table}.csv" ]]; then
        echo "Ya existe resultados/${table}.csv. Renómbralo antes de repetir las mediciones." >&2
        exit 1
    fi
done

echo 'repeticion,tipo,elementos,bytes,rondas,total_s,ida_vuelta_us,ida_estimada_us' > resultados/ping_pong.csv
echo 'repeticion,tipo,modo,procesos,vueltas,token_final,tiempo_rank0_s' > resultados/token_ring.csv
echo 'repeticion,tipo,elementos,bytes,operaciones_por_test,operaciones_total,tiempo_recepcion_s,tiempo_envio_s,suma' > resultados/recepcion_anticipada.csv
echo 'repeticion,tipo,elementos_totales,elementos_chunk,bytes_chunk,numero_chunks,tiempo_total_s,suma' > resultados/pipeline_chunks.csv

record_run() {
    local table="$1"
    local label="$2"
    local repetition="$3"
    shift 3
    local raw="resultados/brutos/${label}_r${repetition}.txt"

    echo "Ejecutando $label (repetición $repetition)"
    "$@" | tee "$raw"
    # Cada programa deja su fila CSV como última línea de salida.
    local row
    row="$(tail -n 1 "$raw")"
    printf '%s,%s\n' "$repetition" "$row" >> "resultados/${table}.csv"
}

for repetition in 1 2 3; do
    for elements in 1 64 1024 16384 65536; do
        # Conservar el protocolo de las mediciones originales: una ronda
        # preparatoria fuera del cronómetro y 1000 rondas medidas.
        record_run ping_pong "ping_pong_e${elements}" "$repetition" \
            mpirun -np 2 ./build/ping_pong 1000 "$elements" --warmup
    done

    for mode in send_recv sendrecv; do
        record_run token_ring "token_ring_${mode}" "$repetition" \
            mpirun --oversubscribe -np 5 ./build/token_ring "$mode" 1000
    done

    for elements in 1 64 1024 16384 65536; do
        record_run recepcion_anticipada "recepcion_e${elements}" "$repetition" \
            mpirun -np 2 ./build/recepcion_anticipada "$elements" 1000
    done

    for chunk in 256 1024 4096 16384 65536; do
        record_run pipeline_chunks "pipeline_c${chunk}" "$repetition" \
            mpirun -np 2 ./build/pipeline_chunks 1048576 "$chunk"
    done
done

echo 'Mediciones guardadas en resultados/*.csv y resultados/brutos/.'
