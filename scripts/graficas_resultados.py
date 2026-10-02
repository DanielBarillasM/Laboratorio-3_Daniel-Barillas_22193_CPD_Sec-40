#!/usr/bin/env python3
"""Genera las cuatro gráficas del informe a partir de los CSV existentes.

No ejecuta MPI ni altera las mediciones. Exporta PDF vectorial y PNG a 300 dpi.
Cada figura muestra las tres corridas, mediana y rango mínimo-máximo observado;
las barras no son intervalos de confianza. Requiere Matplotlib.
"""

import argparse
import contextlib
import io
import statistics
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # Exportación sin ventanas ni backend gráfico interactivo.
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

from analizar_resultados import load_table, summarize


BLUE = "#10385F"
TEAL = "#087E8B"
RAW = "#AD6200"


def canvas(title, ylabel):
    fig, ax = plt.subplots(figsize=(8.2, 4.6), layout="constrained")
    ax.set_title(title, loc="left", fontweight="bold", color=BLUE, pad=15)
    ax.set_ylabel(ylabel)
    ax.grid(axis="y", color="#D9E2EA", linewidth=0.7)
    ax.set_axisbelow(True)
    ax.spines[["top", "right"]].set_visible(False)
    return fig, ax


def save(fig, output, name):
    for extension in ["pdf", "png"]:
        fig.savefig(output / f"{name}.{extension}", dpi=300,
                    bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"Generado: {name}.pdf / {name}.png")


def size_chart(rows, size_column, value_column, factor, title, ylabel,
               output, name, logarithmic_y=False):
    sizes = sorted({int(row[size_column]) for row in rows})
    samples = [[float(row[value_column]) * factor for row in rows
                if int(row[size_column]) == size] for size in sizes]
    medians = [statistics.median(values) for values in samples]
    lower = [median - min(values) for median, values in zip(medians, samples)]
    upper = [max(values) - median for median, values in zip(medians, samples)]
    fig, ax = canvas(title, ylabel)
    ax.set_xscale("log", base=2)
    if logarithmic_y:
        ax.set_yscale("log")
        ax.set_yticks([0.5, 1, 5, 10, 50, 100])
        ax.yaxis.set_major_formatter(FuncFormatter(lambda value, position: f"{value:g}"))
    ax.errorbar(sizes, medians, yerr=[lower, upper], color=BLUE, marker="o",
                linewidth=2, markersize=7, capsize=5,
                label="Mediana y rango mín.-máx.")
    for repetition, marker in enumerate(["o", "^", "s"]):
        ax.scatter([size * (1 + (repetition - 1) * 0.045) for size in sizes],
                   [values[repetition] for values in samples], marker=marker,
                   color=RAW, s=35, alpha=0.8, zorder=3,
                   label="Tres repeticiones" if repetition == 0 else None)
    labels = {4: "4 B", 256: "256 B", 1024: "1 KiB", 4096: "4 KiB",
              16384: "16 KiB", 65536: "64 KiB", 262144: "256 KiB"}
    ax.set_xticks(sizes, [labels[size] for size in sizes])
    ax.set_xlim(sizes[0] / 1.6, sizes[-1] * 1.6)
    ax.set_xlabel("Tamaño de chunk (escala log₂)" if "chunk" in size_column
                  else "Tamaño del mensaje (escala log₂)")
    if not logarithmic_y:
        # Establecer límites DESPUÉS de agregar los datos: set_ylim desactiva
        # el autoescalado y no debe conservar el intervalo inicial de 0..1.
        ax.set_ylim(0, max(max(values) for values in samples) * 1.22)
    ax.legend(frameon=False, loc="upper left" if logarithmic_y else "upper right")
    save(fig, output, name)


def token_chart(rows, output):
    modes = ["send_recv", "sendrecv"]
    samples = [[float(row["tiempo_rank0_s"]) * 1e3 for row in rows
                if row["modo"] == mode] for mode in modes]
    medians = [statistics.median(values) for values in samples]
    fig, ax = canvas("Token Ring | 5 ranks, 1000 vueltas", "Tiempo de rank 0 (ms)")
    bars = ax.bar([0, 1], medians, width=0.5, color=[BLUE, TEAL],
                  edgecolor=BLUE, alpha=0.85, label="Mediana")
    bars[1].set_hatch("//")
    for index, (values, median) in enumerate(zip(samples, medians)):
        ax.errorbar(index, median, yerr=[[median - min(values)], [max(values) - median]],
                    color=BLUE, capsize=6, linewidth=1.5,
                    label="Rango mín.-máx." if index == 0 else None)
        ax.scatter([index - 0.12, index, index + 0.12], values,
                   color=RAW, edgecolor="white", s=55, zorder=3,
                   label="Tres repeticiones" if index == 0 else None)
        ax.text(index, max(values) + 0.13, f"{median:.3f} ms", ha="center",
                color=BLUE, fontweight="bold")
    ax.set_xticks([0, 1], ["MPI_Send + MPI_Recv", "MPI_Sendrecv"])
    ax.set_ylim(0, 4.6)
    ax.legend(loc="upper left", frameon=False, fontsize=10)
    save(fig, output, "02_token_ring")


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resultados", type=Path, default=root / "resultados")
    parser.add_argument("--salida", type=Path, default=root / "informe" / "figuras")
    args = parser.parse_args()
    # Validar el conjunto completo antes de crear cualquier figura.
    with contextlib.redirect_stdout(io.StringIO()):
        summarize(args.resultados)
    args.salida.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                         "axes.titlesize": 14, "axes.labelsize": 11,
                         "xtick.labelsize": 10, "ytick.labelsize": 10,
                         "pdf.fonttype": 42, "axes.unicode_minus": False})
    size_chart(load_table(args.resultados, "ping_pong"), "bytes", "ida_vuelta_us", 1,
               "Ping-Pong | 2 ranks, 1000 rondas", "RTT medio por corrida (µs, escala log)",
               args.salida, "01_ping_pong", logarithmic_y=True)
    token_chart(load_table(args.resultados, "token_ring"), args.salida)
    size_chart(load_table(args.resultados, "recepcion_anticipada"), "bytes",
               "tiempo_recepcion_s", 1e6, "Recepción anticipada | 1000 iteraciones por test",
               "Recepción + cálculo + sondeos (µs)", args.salida, "03_recepcion_anticipada")
    size_chart(load_table(args.resultados, "pipeline_chunks"), "bytes_chunk",
               "tiempo_total_s", 1e3, "Pipeline | 4 MiB totales, ventana de 2 envíos",
               "Comunicación + procesamiento (ms)", args.salida, "04_pipeline_chunks")


if __name__ == "__main__":
    main()
