"""Teste do Situa sem dependências (só Python 3): roda o testa-situa em alguns presets e confere.

  python3 scripts/testar.py <testa-situa>

Gera uma "fala" sintética (pulsos glotais filtrados em sílabas, com pausas), toca pelo processador
e confere: saída finita e sem estouro, o volume que cada etapa promete, a banda do telefone,
o atraso informado pelo codec e que a saída não depende do tamanho do bloco.
"""
import math, os, random, struct, subprocess, sys, tempfile, wave

SR = 48000
TESTA = sys.argv[1]


def gravar(caminho, x):
    with wave.open(caminho, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32767)) for v in x))


def ler(caminho):
    with open(caminho, "rb") as f:
        dados = f.read()
    # WAV float 32 do testa-situa: procura o bloco "data"
    i = dados.index(b"data") + 8
    n = (len(dados) - i) // 4
    return list(struct.unpack("<%df" % n, dados[i:i + 4 * n]))


def fala():
    random.seed(1)
    x, fase, t = [], 0.0, 0
    for silaba in range(14):
        dur = int(SR * random.uniform(0.12, 0.25))
        f0 = random.uniform(110, 190)
        formantes = [random.uniform(400, 800), random.uniform(1000, 2200), random.uniform(2400, 3200)]
        for i in range(dur):
            env = math.sin(math.pi * i / dur) ** 2
            fase += f0 / SR
            v = sum(math.sin(2 * math.pi * k * fase) / k * sum(math.exp(-((k * f0 - fm) / 180) ** 2) for fm in formantes)
                    for k in range(1, 40))
            x.append(0.12 * env * v + 0.004 * random.gauss(0, 1) * env)
        x.extend([0.0] * int(SR * random.uniform(0.03, 0.15)))
    return x


def rms_db(x):
    e = sum(v * v for v in x) / max(1, len(x))
    return 10 * math.log10(e + 1e-20)


def rodar(entrada, saida, *args):
    r = subprocess.run([TESTA, entrada, saida, *args], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("testa-situa falhou: " + r.stderr)
    return r.stdout


def main():
    tmp = tempfile.mkdtemp()
    ent = os.path.join(tmp, "fala.wav")
    x = fala()
    gravar(ent, x)
    base = rms_db(x)
    erros = []

    casos = {
        "sala": (["space=1", "size=6", "decay=0.5", "distance=2.5"], (-4, 1)),
        "telefone": (["device=1", "lowCut=300", "highCut=3400", "codec=1", "comp=1", "drive=20"], (-2, 2)),
        "celular": (["device=1", "lowCut=300", "highCut=3400", "codec=3", "comp=1"], (-2.5, 2.5)),
        "parede": (["space=1", "size=5", "decay=0.5", "distance=2", "barrier=1", "muffle=70", "leak=5"], (-22, -14)),   # a fala sintética tem mais agudo que a real (−12 dB)
    }
    for nome, (args, (lo, hi)) in casos.items():
        sai = os.path.join(tmp, nome + ".wav")
        log = rodar(ent, sai, *args)
        y = ler(sai)
        pico = max(abs(v) for v in y)
        d = rms_db(y[:len(x)]) - base
        print(f"{nome:9} {d:+5.1f} dB  pico {20 * math.log10(pico + 1e-20):+5.1f} dBFS  | {log.strip()}")
        if not all(math.isfinite(v) for v in y) or pico > 1.0:
            erros.append(f"{nome}: saída inválida")
        if not lo <= d <= hi:
            erros.append(f"{nome}: volume {d:+.1f} dB fora de [{lo}, {hi}]")
        if nome == "celular" and "atraso 1272" not in log:
            erros.append("celular: o atraso do codec não é 1272 amostras")

    # o tamanho do bloco não muda o som
    a, b = os.path.join(tmp, "b64.wav"), os.path.join(tmp, "b4096.wav")
    rodar(ent, a, *casos["parede"][0], "--bloco", "64")
    rodar(ent, b, *casos["parede"][0], "--bloco", "4096")
    ya, yb = ler(a), ler(b)
    dif = max(abs(p - q) for p, q in zip(ya, yb))
    print(f"blocos 64 × 4096: diferença máxima {dif:.2e}")
    if dif > 1e-5:
        erros.append("o tamanho do bloco muda o som")

    if erros:
        print("\n".join("ERRO: " + e for e in erros))
        sys.exit(1)
    print("tudo certo")


if __name__ == "__main__":
    main()
