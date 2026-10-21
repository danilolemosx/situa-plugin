// Situa: o motor (o mesmo do protótipo em Python, CLAUDE AVID/SITUA/PROTOTIPO/scripts/situa.py).
//
// DEVICE → SPACE → BARRIER → OUT
//  - DEVICE (não linear) roda amostra a amostra: corte, ressonância, compressor, saturação, codec.
//  - SPACE e BARRIER (lineares) viram uma resposta ao impulso só: reflexões pela geometria da sala,
//    cauda com decay por banda, ar (ISO 9613-1), perda do grave de proximidade e a parede (lei da
//    massa). Essa resposta é recalculada quando um knob muda e tocada por convolução.

#pragma once

#include "Convolucao.h"

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <random>
#include <vector>

#if SITUA_COM_OPUS
struct OpusEncoder;
struct OpusDecoder;
#endif

namespace situa
{

enum Codec { codecOff = 0, codecLandline, codec2G, codec3G, codecHD, codecVoip, numCodecs };

struct Ajustes
{
    bool device = false, space = false, barrier = false, airPlus = false, cabin = false, comp = false;
    float lowCut = 20.0f, highCut = 20000.0f, drive = 0.0f;
    int codec = codecOff;
    float peakFreq = 1000.0f, peakGain = 0.0f, peakQ = 1.0f;
    float size = 6.0f, decay = 0.6f, distance = 2.0f, lf = 1.0f, hf = 0.6f;
    float muffle = 60.0f, leak = 20.0f;
};

// D/R alvo em dB, já com a convenção de cinema (+10 dB)
float drAlvo (const Ajustes& a);

// a parte linear inteira (SPACE + ar + BARRIER), com os ganhos já aplicados
std::vector<float> gerarResposta (const Ajustes& a, double taxa);

// tudo o que muda a resposta ao impulso (para saber quando recalcular)
bool mesmaResposta (const Ajustes& x, const Ajustes& y);

class Biquad
{
public:
    void passaAltas (double f, double q, double taxa);
    void passaBaixas (double f, double q, double taxa);
    void sino (double f, double ganhoDb, double q, double taxa);
    void prateleiraAgudos (double f, double ganhoDb, double q, double taxa);
    void passa() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    float processar (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void zerar() { z1 = z2 = 0; }

private:
    void definir (double nb0, double nb1, double nb2, double na0, double na1, double na2);
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

// DEVICE de um canal
class Aparelho
{
public:
    Aparelho();
    ~Aparelho();
    void preparar (double taxa);
    void ajustar (const Ajustes& a);
    float processar (float x) noexcept;
    void zerar();
    // atraso do codec escolhido (0 sem codec ou no LANDLINE)
    static int atraso (int codec, double taxa);

private:
    float codecAmostra (float x) noexcept;
    float landline (float x) noexcept;

    double taxa = 48000.0;
    Ajustes aj;
    Biquad hp[2], lp[2], pico, posLp, antiAlias[2], reconstrucao[2], kEntrada[2], kSaida[2];
    bool usaHp = false, usaLp = false, usaPico = false;

    // compressor (RMS, um tempo só, como no protótipo)
    double envComp = 0.0, coefComp = 0.0;
    // saturação: o ganho de entrada segue o pico (o protótipo usava o pico do arquivo inteiro)
    double picoSeguido = 0.0, quedaPico = 0.0;
    // nível: cada etapa muda o timbre, não o volume
    double envEntrada = 0.0, envSaida = 0.0, coefNivel = 0.0, ganhoNivel = 1.0, coefSuave = 0.0;
    double ganhoEstatico = 1.0;

    // LANDLINE: 8 kHz por retenção + A-law de 8 bits
    double faseRetencao = 0.0;
    float retido = 0.0f;

    // Opus (3G, HD, VOIP; 2G no lugar do GSM)
#if SITUA_COM_OPUS
    OpusEncoder* codificador = nullptr;
    OpusDecoder* decodificador = nullptr;
#endif
    int codecConfigurado = -1;
    std::vector<float> entradaQuadro, saidaQuadro, quadroDecodificado, linhaReferencia;
    int posReferencia = 0, atrasoCodec = 0;
    int posEntrada = 0, posSaida = 0;
    std::mt19937 sorteio { 7 };
    int perdidosSeguidos = 0;
};

} // namespace situa
