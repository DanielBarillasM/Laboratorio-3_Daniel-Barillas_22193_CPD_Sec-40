#!/usr/bin/env python3
"""Valida los CSV existentes y resume las repeticiones, sin ejecutar MPI.

Uso: python3 scripts/analizar_resultados.py [carpeta_resultados]
La desviación estándar es muestral (n-1). No se eliminan observaciones ni
se realizan pruebas de significancia con tres repeticiones por grupo.
Solo se leen los archivos: el programa no sobrescribe mediciones.
"""

import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path


def load_table(directory, name):
    """Leer una tabla y rechazar filas incompletas o sin tipo esperado."""
    with (directory / f"{name}.csv").open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or any(None in row or any(value in (None, "") for value in row.values())
                       for row in rows):
        raise ValueError(f"{name}: tabla vacía o filas incompletas")
    if any(row["tipo"] != name for row in rows):
        raise ValueError(f"{name}: tipo de experimento incorrecto")
    return rows


def integer(row, column):
    return int(row[column])


def time_value(row, column, factor=1.0):
    value = float(row[column]) * factor
    if not math.isfinite(value) or value <= 0:
        raise ValueError(f"Tiempo inválido en {column}: {value}")
    return value


def groups(rows, key, expected):
    """Exigir las configuraciones y repeticiones del barrido documentado."""
    grouped = defaultdict(list)
    for row in rows:
        grouped[key(row)].append(row)
    if set(grouped) != set(expected):
        raise ValueError("Las configuraciones no coinciden con el barrido")
    for label, samples in grouped.items():
        if sorted(integer(row, "repeticion") for row in samples) != [1, 2, 3]:
            raise ValueError(f"{label}: se requieren repeticiones 1, 2 y 3 sin duplicados")
    return [(label, grouped[label]) for label in expected]


def describe(values):
    """Formato compartido por las tablas del README; conservar todas las filas."""
    return (f"{statistics.median(values):.3f} | "
            f"{statistics.mean(values):.3f} ± {statistics.stdev(values):.3f} | "
            f"{min(values):.3f}–{max(values):.3f}")


def pattern_sum(elements, period):
    full, remainder = divmod(elements, period)
    return full * period * (period - 1) // 2 + remainder * (remainder - 1) // 2


def require(condition, message):
    if not condition:
        raise ValueError(message)


def summarize(directory):
    sizes = [1, 64, 1024, 16384, 65536]
    chunks = [256, 1024, 4096, 16384, 65536]
    ping = load_table(directory, "ping_pong")
    token = load_table(directory, "token_ring")
    early = load_table(directory, "recepcion_anticipada")
    pipeline = load_table(directory, "pipeline_chunks")

    print("## Ping-Pong · RTT en µs\n")
    print("| Enteros | Bytes | Mediana | Media ± DE | Mín.–máx. |")
    print("|---:|---:|---:|---:|---:|")
    ping_medians = []
    for elements, samples in groups(ping, lambda row: integer(row, "elementos"), sizes):
        for row in samples:
            require(integer(row, "rondas") == 1000, "Ping-Pong: rondas distintas")
            require(integer(row, "bytes") == 4 * elements, "Ping-Pong: bytes distintos")
            rtt = time_value(row, "total_s", 1e6) / integer(row, "rondas")
            require(abs(rtt - time_value(row, "ida_vuelta_us")) <= 0.000501,
                    "Ping-Pong: RTT inconsistente con el tiempo total")
            require(abs(rtt / 2 - time_value(row, "ida_estimada_us")) <= 0.000501,
                    "Ping-Pong: ida estimada inconsistente")
        values = [time_value(row, "ida_vuelta_us") for row in samples]
        ping_medians.append(statistics.median(values))
        print(f"| {elements} | {elements * 4} | {describe(values)} |")

    print("\n## Token Ring · tiempo de rank 0 en ms\n")
    print("| Variante | Mediana | Media ± DE | Mín.–máx. | Token final |")
    print("|---|---:|---:|---:|---:|")
    token_medians = []
    for mode, samples in groups(token, lambda row: row["modo"], ["send_recv", "sendrecv"]):
        for row in samples:
            require(integer(row, "procesos") == 5 and integer(row, "vueltas") == 1000,
                    "Token Ring: configuración distinta")
            require(integer(row, "token_final") == 5000, "Token Ring: token incorrecto")
        values = [time_value(row, "tiempo_rank0_s", 1e3) for row in samples]
        token_medians.append(statistics.median(values))
        print(f"| `{mode}` | {describe(values)} | 5000 |")

    print("\n## Recepción anticipada · tiempo de rank 1 en µs\n")
    print("| Enteros | Bytes | Mediana | Media ± DE | Mín.–máx. | Envío mediano (µs) | Operaciones mín.–máx. | Suma |")
    print("|---:|---:|---:|---:|---:|---:|---:|---:|")
    early_medians = []
    for elements, samples in groups(early, lambda row: integer(row, "elementos"), sizes):
        for row in samples:
            require(integer(row, "bytes") == elements * 4 and
                    integer(row, "operaciones_por_test") == 1000,
                    "Recepción anticipada: configuración distinta")
            require(integer(row, "operaciones_total") > 0 and
                    integer(row, "operaciones_total") % 1000 == 0,
                    "Recepción anticipada: lotes de trabajo incorrectos")
            require(integer(row, "suma") == pattern_sum(elements, 1009),
                    "Recepción anticipada: suma incorrecta")
        values = [time_value(row, "tiempo_recepcion_s", 1e6) for row in samples]
        sending = [time_value(row, "tiempo_envio_s", 1e6) for row in samples]
        operations = [integer(row, "operaciones_total") for row in samples]
        early_medians.append(statistics.median(values))
        print(f"| {elements} | {elements * 4} | {describe(values)} | "
              f"{statistics.median(sending):.3f} | {min(operations)}–{max(operations)} | "
              f"{pattern_sum(elements, 1009)} |")

    print("\n## Pipeline · tiempo total en ms\n")
    print("| Enteros/chunk | Bytes/chunk | Chunks | Mediana | Media ± DE | Mín.–máx. |")
    print("|---:|---:|---:|---:|---:|---:|")
    pipeline_medians = []
    for chunk, samples in groups(pipeline, lambda row: integer(row, "elementos_chunk"), chunks):
        for row in samples:
            require(integer(row, "elementos_totales") == 1048576 and
                    integer(row, "bytes_chunk") == chunk * 4 and
                    integer(row, "numero_chunks") == 1048576 // chunk,
                    "Pipeline: configuración distinta")
            require(integer(row, "suma") == pattern_sum(1048576, 1000),
                    "Pipeline: suma incorrecta")
        values = [time_value(row, "tiempo_total_s", 1e3) for row in samples]
        pipeline_medians.append(statistics.median(values))
        print(f"| {chunk} | {chunk * 4} | {1048576 // chunk} | {describe(values)} |")

    print(f"\nFilas validadas: {len(ping) + len(token) + len(early) + len(pipeline)}")
    print("Configuraciones: 17; repeticiones por configuración: 3; datos faltantes: 0")
    print(f"RTT mayor/menor mensaje: {ping_medians[-1] / ping_medians[0]:.3f}×")
    print(f"Sendrecv / Send-Recv (medianas): {token_medians[1] / token_medians[0]:.3f}×")
    print(f"Aumento Sendrecv: {(token_medians[1] / token_medians[0] - 1) * 100:.2f}%")
    print(f"Recepción mayor/menor mensaje: {early_medians[-1] / early_medians[0]:.3f}×")
    print(f"Pipeline chunks pequeños/grandes: {pipeline_medians[0] / pipeline_medians[-1]:.3f}×")
    print(f"Reducción tiempo pipeline: {(1 - pipeline_medians[-1] / pipeline_medians[0]) * 100:.2f}%")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resultados", nargs="?", type=Path,
                        default=Path(__file__).resolve().parents[1] / "resultados")
    args = parser.parse_args()
    try:
        summarize(args.resultados)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"Error al validar resultados: {error}\n")


if __name__ == "__main__":
    main()
