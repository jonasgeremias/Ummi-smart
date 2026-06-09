#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
calc_umidade.py — UR do HIH5030/5031 a partir da tensao no MEIO do divisor.

Hardware (Ummi Control):
  - HIH alimentado em Vcc (3,3 V tipico; pode ser 5 V) — saida RATIOMETRICA:
        Vout = Vcc * (0,00636*RH + 0,1515)
  - Divisor de entrada:  sinal --[1k]--+--[1k8]--GND   ('+' = ADC do STM)
        Vno = Vsensor * 1k8/(1k+1k8) = Vsensor * 1,8/2,8
  - Firmware: dUR = (mV_no * GUR - ZUR)/100   (na tensao POS-divisor)

Uso:
  python calc_umidade.py 1.06                 # UR (Vcc padrao = 3.3 V)
  python calc_umidade.py 1.06 --vcc 5         # UR com Vcc=5 V
  python calc_umidade.py 1.06 25 --vcc 3.3    # UR + temperatura (C)
  python calc_umidade.py --cal 1.06 60        # 1 ponto -> GUR/ZUR (usa Vcc)
  python calc_umidade.py --cal2 0.49 0 1.06 60  # 2 pontos -> GUR/ZUR
  python calc_umidade.py                      # interativo
"""

import sys
import argparse

# ---------------- Hardware ----------------
R_TOP = 1000.0      # 1k em serie
R_BOTTOM = 1800.0   # 1k8 para GND
VCC_PADRAO = 5.0    # alimentacao do HIH (medido 5 V no pino)

GANHO = R_BOTTOM / (R_TOP + R_BOTTOM)   # Vno/Vsensor = 1,8/2,8 = 0,6429


def vsensor(vno):
    return vno / GANHO


def ur_ideal(vno, vcc, temp_c=None):
    """UR (%) pela folha do HIH (ratiometrico ao Vcc)."""
    rh = ((vsensor(vno) / vcc) - 0.1515) / 0.00636
    if temp_c is not None:
        rh = rh / (1.0546 - 0.00216 * temp_c)
    return max(0.0, min(100.0, rh))


def ur_firmware(vno_mv, gur, zur):
    """UR (%) como o firmware (inteiro, com janela 0/100)."""
    calc = vno_mv * gur - zur
    if calc < 0:
        calc = 0
    dur = int(calc) // 100
    if dur > 1000:
        dur = 1000
    return dur / 10.0


def gur_teorico(vcc):
    """Inclinacao teorica (GUR) para o HIH neste Vcc, pos-divisor.
       gur/100 = 10 / (d(mV_no)/dRH) ; d(mV_no)/dRH = Vcc*0,00636*GANHO*1000."""
    return round(244.6 / vcc)   # 5V->49 ; 3,3V->74


def calibra_1ponto(vno, rh, vcc):
    """GUR teorico do Vcc + ZUR ajustado para passar no ponto medido."""
    gur = gur_teorico(vcc)
    zur = round(vno * 1000.0 * gur - rh * 1000.0)
    return gur, zur


def calibra_2pontos(vno1, rh1, vno2, rh2):
    """Reta por 2 pontos (independe do Vcc)."""
    mv1, mv2 = vno1 * 1000.0, vno2 * 1000.0
    gur = round(1000.0 * (rh2 - rh1) / (mv2 - mv1))
    zur = round(mv1 * gur - rh1 * 1000.0)
    return gur, zur


def mostra(vno, vcc, temp_c=None, gur=None, zur=None):
    if gur is None:
        gur = gur_teorico(vcc)
    if zur is None:
        # ZUR teorico: 0% no Vout=0,1515*Vcc
        zur = round(0.1515 * vcc * GANHO * 1000.0 * gur)
    vno_mv = vno * 1000.0
    print()
    print(f"  Vcc do sensor      : {vcc:.2f} V")
    print(f"  Tensao no no (ADC) : {vno:.3f} V  ({vno_mv:.0f} mV)")
    print(f"  Tensao do sensor   : {vsensor(vno):.3f} V")
    if temp_c is not None:
        print(f"  Temperatura        : {temp_c:.1f} C")
    print("  " + "-" * 46)
    print(f"  UR (folha HIH)     : {ur_ideal(vno, vcc, temp_c):5.1f} %")
    urf = ur_firmware(vno_mv, gur, zur)
    print(f"  UR (firmware)      : {urf:5.1f} %  (GUR={gur}, ZUR={zur}) -> display {int(urf):03d}")
    print()


def f(x):
    return float(str(x).replace(",", "."))


def main():
    p = argparse.ArgumentParser(add_help=True, description="UR do HIH a partir da tensao no no.")
    p.add_argument("v", nargs="?", help="tensao no no (V)")
    p.add_argument("t", nargs="?", help="temperatura (C), opcional")
    p.add_argument("--vcc", default=VCC_PADRAO, help="Vcc do sensor (V), padrao 3.3")
    p.add_argument("--gur", default=None, help="GUR manual")
    p.add_argument("--zur", default=None, help="ZUR manual")
    p.add_argument("--cal", nargs=2, metavar=("V", "RH"),
                   help="1 ponto: tensao e UR real -> GUR/ZUR (usa --vcc)")
    p.add_argument("--cal2", nargs=4, metavar=("V1", "RH1", "V2", "RH2"),
                   help="2 pontos -> GUR/ZUR (independe do Vcc)")
    a = p.parse_args()

    vcc = f(a.vcc)

    if a.cal2:
        v1, r1, v2, r2 = map(f, a.cal2)
        gur, zur = calibra_2pontos(v1, r1, v2, r2)
        print(f"\n  Calibracao por 2 pontos:")
        print(f"    ({v1:.3f} V -> {r1:.1f}%) e ({v2:.3f} V -> {r2:.1f}%)")
        print(f"    >>> GUR = {gur}   ZUR = {zur}\n")
        return
    if a.cal:
        v1, r1 = f(a.cal[0]), f(a.cal[1])
        gur, zur = calibra_1ponto(v1, r1, vcc)
        print(f"\n  Calibracao 1 ponto @ Vcc={vcc:.2f} V:  {v1:.3f} V -> {r1:.1f}%")
        print(f"    >>> GUR = {gur}   ZUR = {zur}")
        mostra(v1, vcc, gur=gur, zur=zur)
        return

    gur = round(f(a.gur)) if a.gur is not None else None
    zur = round(f(a.zur)) if a.zur is not None else None

    if a.v is not None:
        temp = f(a.t) if a.t is not None else None
        mostra(f(a.v), vcc, temp, gur, zur)
        return

    print("=== Calculadora de Umidade (HIH + divisor 1k/1k8) ===")
    print(f"Vcc do sensor = {vcc:.2f} V (use --vcc para mudar)")
    try:
        while True:
            s = input("\nTensao no no (V) [Enter sai]: ").strip()
            if not s:
                break
            ts = input("Temperatura (C) [Enter ignora]: ").strip()
            temp = f(ts) if ts else None
            mostra(f(s), vcc, temp, gur, zur)
    except (EOFError, ValueError):
        pass
    print("\nSaindo.")


if __name__ == "__main__":
    main()
