// Situa: convolução sem atraso, em partições.
//
// As primeiras P amostras da resposta vão por convolução direta (sem atraso); o resto vai por FFT
// em partições de P, que já nasce atrasado P amostras — por isso a soma não tem atraso nenhum.
// Trocar a resposta não pode estalar: a nova entra com um cruzamento de 40 ms.

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>
#include <vector>

namespace situa
{

struct RespostaPreparada
{
    static constexpr int P = 512;          // tamanho da partição
    static constexpr int ordemFFT = 10;    // FFT de 2P
    std::vector<float> cabeca;             // h[0..P)
    std::vector<std::vector<float>> partes; // espectro de cada pedaço h[P + mP .. P + (m+1)P), formato do JUCE
    int comprimento = 0;

    static std::shared_ptr<RespostaPreparada> preparar (const std::vector<float>& h);
};

// estado da convolução de um canal com uma resposta
class CanalConvolucao
{
public:
    void iniciar (std::shared_ptr<const RespostaPreparada> r);
    float processar (float x) noexcept;

private:
    void quadro() noexcept;

    std::shared_ptr<const RespostaPreparada> resposta;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> historia;     // últimas P entradas, duplicadas (anel)
    int posHistoria = 0;
    std::vector<float> quadroAnterior, quadroAtual, saidaCauda, trabalho, soma;
    std::vector<std::vector<float>> linha; // espectros das entradas passadas (anel)
    int posLinha = 0, pos = 0;
    float escalaInversa = 1.0f;
};

// convolução de vários canais com troca suave de resposta
class Convolucao
{
public:
    void preparar (int canais);
    // troca imediata (sem cruzamento): antes de tocar, ou no render do AudioSuite
    void definirJa (std::shared_ptr<RespostaPreparada> r);
    // troca durante a reprodução (fora da thread de áudio)
    void definir (std::shared_ptr<RespostaPreparada> r);
    void processar (juce::AudioBuffer<float>& audio, int canais) noexcept;
    // a resposta antiga sai da thread de áudio por aqui (chamar na thread de mensagens)
    void descartarAntigas();

private:
    juce::SpinLock trava;
    int canaisPreparados = 2;
    std::vector<CanalConvolucao> atual, entrando, pendente, lixeira;
    bool temPendente = false;
    std::atomic<bool> lixeiraCheia { false };
    int restanteCruzamento = 0;
    static constexpr int cruzamento = 1920;   // 40 ms a 48 kHz
};

} // namespace situa
