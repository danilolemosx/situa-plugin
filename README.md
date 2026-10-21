# Situa

Coloca o diálogo num lugar: telefone, rádio, sala grande, igreja, longe na rua, através da parede.
Formatos: AAX (Media Composer e Pro Tools, ao vivo e AudioSuite), VST3 e AU (Premiere, Resolve, Logic).

A cadeia é **DEVICE → SPACE → BARRIER → OUT**:

- **DEVICE:** corte de banda, ressonância de alto-falante, compressor, saturação (Spiral do Airwindows)
  e codec: LANDLINE (G.711 A-law a 8 kHz), 2G, 3G, HD e VOIP (Opus, com perda de pacotes no VOIP).
- **SPACE:** reflexões calculadas pela geometria da sala, cauda com decay separado por banda,
  absorção do ar (ISO 9613-1) e a proporção entre voz direta e reverb pela distância crítica de Sabine.
  A sala não muda o volume da voz; só a distância baixa (−3 dB por dobra, até −8 dB).
- **BARRIER:** parede pela lei da massa (6 a 12 dB/oitava), fresta (LEAK) em paralelo e queda de 4 a 14 dB.

SPACE e BARRIER viram uma resposta ao impulso só, recalculada quando um knob muda e tocada por uma
convolução própria **sem atraso**. Só os codecs Opus atrasam (1272 amostras, 26,5 ms a 48 kHz); o
atraso é informado ao programa, que compensa. Em outras taxas, os codecs de celular viram o LANDLINE.

Os 26 presets ficam na janela (`juce/ui/app.js`) e são os mesmos do protótipo em Python
(`CLAUDE AVID/SITUA/PROTOTIPO/scripts/situa.py`). O estudo que deu origem a tudo está em
`CLAUDE AVID/SITUA/LEVANTAMENTO.md`.

## Compilar e testar (Mac, Apple Silicon)

```
cmake -B ~/.cache/situa-aax/build -S juce -DCMAKE_BUILD_TYPE=Release
cmake --build ~/.cache/situa-aax/build --target Situa_VST3 Situa_AU Situa_AAX testa-situa -j 10
```

Compile fora do HD externo (o caminho tem espaços). O JUCE é o 8.0.9 de `~/.cache/decanta-aax/JUCE`
(`-DJUCE_DIR=…` para outro lugar). O Opus 1.5.2 é baixado e compilado junto na primeira configuração.
`-DSITUA_UNIVERSAL=ON` gera Apple Silicon + Intel. O GitHub (`.github/workflows/plugin.yml`) compila
Mac universal e Windows a cada envio e roda os testes.

- `python3 scripts/testar.py <testa-situa>`: volume de quatro presets, banda, atraso do codec e blocos.
- `testa-situa <entrada.wav> <saida.wav> param=valor …` toca o arquivo pelo processador em blocos.
- `SITUA_TESTE_JANELA=/tmp/j.json SITUA_TESTE_PRESET="Through Wall" testa-situa --janela <Situa.vst3> 6`
  abre a janela, carrega o preset por ela e lista como ficaram os parâmetros.
- `CLAUDE AVID/SITUA/PROTOTIPO/scripts/conferir_plugin.py` compara os 26 presets com o protótipo
  (volume, banda, timbre, sincronia e RT60).
- pluginval nível 7 (VST3) e `auval -v aufx Situ DnLm` (AU).

O instalador sai de `scripts/empacotar-mac.sh <pasta com os plugins> <versão>`: assina VST3 e AU com o
certificado do Mac e o AAX com as ferramentas da PACE (iLok conectado; dados em `scripts/assinatura.local`,
fora do repositório).

## Licença e créditos

AGPL-3.0, como o Decanta e o Apura (o JUCE gratuito pede isso).

- Saturação Spiral: Airwindows, Chris Johnson (MIT) — `licencas/airwindows.txt`.
- Opus: Xiph.Org e colaboradores (BSD) — `licencas/opus.txt`.
- Valores de referência dos presets: estudados nos presets do Dragonfly Reverb (Michael Willis), sem código copiado.
