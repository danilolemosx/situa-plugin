// Situa: o processador do plugin (parâmetros, DEVICE por canal, convolução da parte linear).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Motor.h"

#include <array>
#include <atomic>

namespace ID
{
    // visíveis na janela
    static constexpr const char* lowCut = "lowCut";
    static constexpr const char* highCut = "highCut";
    static constexpr const char* drive = "drive";
    static constexpr const char* codec = "codec";
    static constexpr const char* muffle = "muffle";
    static constexpr const char* leak = "leak";
    static constexpr const char* size = "size";
    static constexpr const char* decay = "decay";
    static constexpr const char* distance = "distance";
    static constexpr const char* mix = "mix";
    static constexpr const char* output = "output";
    static constexpr const char* device = "device";
    static constexpr const char* space = "space";
    static constexpr const char* barrier = "barrier";
    static constexpr const char* airPlus = "airPlus";
    static constexpr const char* mono = "mono";
    static constexpr const char* bypass = "bypass";
    // definidos pelos presets
    static constexpr const char* comp = "comp";
    static constexpr const char* cabin = "cabin";
    static constexpr const char* lf = "lf";
    static constexpr const char* hf = "hf";
    static constexpr const char* peakFreq = "peakFreq";
    static constexpr const char* peakGain = "peakGain";
    static constexpr const char* peakQ = "peakQ";
    static constexpr const char* preset = "preset";
}

class SituaProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SituaProcessor();
    ~SituaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Situa"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;

    // para os testes: espera a resposta ao impulso dos ajustes atuais ficar pronta
    void aplicarAjustesAgora();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout criarParametros();
    situa::Ajustes lerAjustes() const;
    void processarPedaco (juce::AudioBuffer<float>& buffer, int canais);
    void timerCallback() override;
    void atualizarAtraso (int codec);

    std::atomic<float>* pLowCut, * pHighCut, * pDrive, * pCodec, * pMuffle, * pLeak, * pSize, * pDecay, * pDistance,
        * pMix, * pOutput, * pDevice, * pSpace, * pBarrier, * pAirPlus, * pMono, * pBypass, * pComp, * pCabin,
        * pLf, * pHf, * pPeakFreq, * pPeakGain, * pPeakQ;

    double taxa = 48000.0;
    std::array<situa::Aparelho, 2> aparelhos;
    situa::Ajustes ajustesAparelho;
    situa::Convolucao convolucao;

    // a resposta ao impulso é recalculada fora da thread de áudio
    class Gerador;
    std::unique_ptr<Gerador> gerador;
    situa::Ajustes ajustesResposta;      // os da resposta em uso (ou pedida)
    juce::CriticalSection travaResposta;
    std::atomic<int> versaoResposta { 0 };

    // atraso (só o codec atrasa); a parte seca acompanha para o MIX e o BYPASS
    std::atomic<int> atrasoAtual { 0 };
    juce::AudioBuffer<float> linhaSeca;
    int posSeca = 0;
    juce::AudioBuffer<float> seco;
    juce::SmoothedValue<float> ganhoSaida, mixSuave;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SituaProcessor)
};
