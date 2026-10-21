#include "PluginProcessor.h"

#if ! SITUA_SEM_JANELA
 #include "PluginEditor.h"
#endif

#include <cmath>

using namespace juce;

// nomes dos presets (a lista com os valores fica na janela, ui/app.js)
static const StringArray nomesPresets {
    "INIT",
    "Small Room", "Living Room", "Large Hall", "Church", "Bathroom", "Stairwell", "Car Interior", "Warehouse",
    "Across the Room", "Far Away · Indoor", "Far Away · Street", "Shouting Distance",
    "Landline Phone", "Cell Phone", "Cell Phone · HD", "Video Call", "Speakerphone", "Walkie-Talkie", "AM Radio",
    "TV in Next Room", "PA Speaker", "Megaphone",
    "Through Wall", "Through Door", "Next Apartment", "Underwater"
};

// faixa logarítmica (os knobs de frequência, tamanho, tempo e distância); a janela usa a mesma conta
static NormalisableRange<float> faixaLog (float de, float ate)
{
    return { de, ate,
             [] (float s, float e, float n) { return s * std::pow (e / s, n); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             nullptr };
}

AudioProcessorValueTreeState::ParameterLayout SituaProcessor::criarParametros()
{
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    using B = AudioParameterBool;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto texto = [] (std::function<String (float)> f) { return A().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto pct = texto ([] (float v) { return String (roundToInt (v)) + "%"; });
    auto db = texto ([] (float v) { const auto r = std::round (v * 10) / 10; return (r > 0 ? "+" : "") + String (r == 0 ? 0.0f : r, 1) + " dB"; });
    auto hz = [] (float v) { return v >= 1000 ? String (v / 1000, v >= 10000 ? 0 : 1) + " kHz" : String (roundToInt (v)) + " Hz"; };

    p.push_back (std::make_unique<F> (ParameterID { ID::lowCut, 1 }, "Low Cut", faixaLog (20, 1000), 20.0f,
                                      texto ([hz] (float v) { return v <= 21 ? String ("OFF") : hz (v); })));
    p.push_back (std::make_unique<F> (ParameterID { ID::highCut, 1 }, "High Cut", faixaLog (1000, 20000), 20000.0f,
                                      texto ([hz] (float v) { return v >= 19800 ? String ("OFF") : hz (v); })));
    p.push_back (std::make_unique<F> (ParameterID { ID::drive, 1 }, "Drive", NormalisableRange<float> (0, 100), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ID::codec, 1 }, "Codec",
                                                         StringArray { "OFF", "LANDLINE", "2G", "3G", "HD", "VOIP" }, 0));
    p.push_back (std::make_unique<F> (ParameterID { ID::muffle, 1 }, "Muffle", NormalisableRange<float> (0, 100), 60.0f, pct));
    p.push_back (std::make_unique<F> (ParameterID { ID::leak, 1 }, "Leak", NormalisableRange<float> (0, 100), 20.0f, pct));
    p.push_back (std::make_unique<F> (ParameterID { ID::size, 1 }, "Size", faixaLog (1.5f, 80), 6.0f,
                                      texto ([] (float v) { return v >= 60 ? String ("OPEN") : String (v, v < 10 ? 1 : 0) + " m"; })));
    p.push_back (std::make_unique<F> (ParameterID { ID::decay, 1 }, "Decay", faixaLog (0.05f, 8), 0.6f,
                                      texto ([] (float v) { return String (v, v < 1 ? 2 : 1) + " s"; })));
    p.push_back (std::make_unique<F> (ParameterID { ID::distance, 1 }, "Distance", faixaLog (0.3f, 50), 2.0f,
                                      texto ([] (float v) { return String (v, v < 10 ? 1 : 0) + " m"; })));
    p.push_back (std::make_unique<F> (ParameterID { ID::mix, 1 }, "Mix", NormalisableRange<float> (0, 100), 100.0f, pct));
    p.push_back (std::make_unique<F> (ParameterID { ID::output, 1 }, "Output", NormalisableRange<float> (-24, 12), 0.0f, db));
    p.push_back (std::make_unique<B> (ParameterID { ID::device, 1 }, "Device", false));
    p.push_back (std::make_unique<B> (ParameterID { ID::space, 1 }, "Space", false));
    p.push_back (std::make_unique<B> (ParameterID { ID::barrier, 1 }, "Barrier", false));
    p.push_back (std::make_unique<B> (ParameterID { ID::airPlus, 1 }, "Air+", false));
    p.push_back (std::make_unique<B> (ParameterID { ID::mono, 1 }, "Mono", false));
    p.push_back (std::make_unique<B> (ParameterID { ID::bypass, 1 }, "Bypass", false));
    // definidos pelos presets (sem knob na janela)
    p.push_back (std::make_unique<B> (ParameterID { ID::comp, 1 }, "Device Compressor", false));
    p.push_back (std::make_unique<B> (ParameterID { ID::cabin, 1 }, "Cabin Gain", false));
    p.push_back (std::make_unique<F> (ParameterID { ID::lf, 1 }, "Low Decay ×", NormalisableRange<float> (0.5f, 2), 1.0f));
    p.push_back (std::make_unique<F> (ParameterID { ID::hf, 1 }, "High Decay ×", NormalisableRange<float> (0.2f, 1), 0.6f));
    p.push_back (std::make_unique<F> (ParameterID { ID::peakFreq, 1 }, "Speaker Peak Freq", faixaLog (100, 6000), 1000.0f, A().withLabel ("Hz")));
    p.push_back (std::make_unique<F> (ParameterID { ID::peakGain, 1 }, "Speaker Peak Gain", NormalisableRange<float> (-12, 12), 0.0f, db));
    p.push_back (std::make_unique<F> (ParameterID { ID::peakQ, 1 }, "Speaker Peak Q", NormalisableRange<float> (0.3f, 4), 1.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ID::preset, 1 }, "Preset", nomesPresets, 0,
                                                         AudioParameterChoiceAttributes().withAutomatable (false)));
    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------------------------------------
// a resposta ao impulso é calculada numa thread própria (pode levar dezenas de ms nos espaços grandes)

class SituaProcessor::Gerador : public Thread
{
public:
    explicit Gerador (SituaProcessor& p) : Thread ("Situa IR"), dono (p) { startThread(); }
    ~Gerador() override
    {
        signalThreadShouldExit();
        pedido.signal();
        stopThread (4000);
    }

    void pedir (const situa::Ajustes& a, double taxa, int versao)
    {
        {
            const ScopedLock l (trava);
            ajustes = a;
            taxaPedida = taxa;
            versaoPedida = versao;
            temPedido = true;
        }
        pedido.signal();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            pedido.wait (-1);
            if (threadShouldExit())
                return;
            situa::Ajustes a;
            double taxa;
            int versao;
            {
                const ScopedLock l (trava);
                if (! temPedido)
                    continue;
                a = ajustes;
                taxa = taxaPedida;
                versao = versaoPedida;
                temPedido = false;
            }
            auto r = situa::RespostaPreparada::preparar (situa::gerarResposta (a, taxa));
            // só entra se ninguém pediu outra depois (o render do AudioSuite pode ter feito a sua)
            if (versao == dono.versaoResposta.load())
                dono.convolucao.definir (std::move (r));
        }
    }

private:
    SituaProcessor& dono;
    WaitableEvent pedido;
    CriticalSection trava;
    situa::Ajustes ajustes;
    double taxaPedida = 48000.0;
    int versaoPedida = 0;
    bool temPedido = false;
};

// ---------------------------------------------------------------------------------------------

SituaProcessor::SituaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SITUA", criarParametros())
{
    auto ler = [this] (const char* id) { return state.getRawParameterValue (id); };
    pLowCut = ler (ID::lowCut); pHighCut = ler (ID::highCut); pDrive = ler (ID::drive); pCodec = ler (ID::codec);
    pMuffle = ler (ID::muffle); pLeak = ler (ID::leak); pSize = ler (ID::size); pDecay = ler (ID::decay);
    pDistance = ler (ID::distance); pMix = ler (ID::mix); pOutput = ler (ID::output); pDevice = ler (ID::device);
    pSpace = ler (ID::space); pBarrier = ler (ID::barrier); pAirPlus = ler (ID::airPlus); pMono = ler (ID::mono);
    pBypass = ler (ID::bypass); pComp = ler (ID::comp); pCabin = ler (ID::cabin); pLf = ler (ID::lf); pHf = ler (ID::hf);
    pPeakFreq = ler (ID::peakFreq); pPeakGain = ler (ID::peakGain); pPeakQ = ler (ID::peakQ);
    gerador = std::make_unique<Gerador> (*this);
    startTimerHz (10);
}

SituaProcessor::~SituaProcessor()
{
    stopTimer();
    gerador.reset();
}

situa::Ajustes SituaProcessor::lerAjustes() const
{
    situa::Ajustes a;
    a.device = pDevice->load() > 0.5f;
    a.space = pSpace->load() > 0.5f;
    a.barrier = pBarrier->load() > 0.5f;
    a.airPlus = pAirPlus->load() > 0.5f;
    a.cabin = pCabin->load() > 0.5f;
    a.comp = pComp->load() > 0.5f;
    a.lowCut = pLowCut->load();
    a.highCut = pHighCut->load();
    a.drive = pDrive->load();
    a.codec = roundToInt (pCodec->load());
    a.peakFreq = pPeakFreq->load();
    a.peakGain = pPeakGain->load();
    a.peakQ = pPeakQ->load();
    a.size = pSize->load();
    a.decay = pDecay->load();
    a.distance = pDistance->load();
    a.lf = pLf->load();
    a.hf = pHf->load();
    a.muffle = pMuffle->load();
    a.leak = pLeak->load();
    return a;
}

static bool mesmoAparelho (const situa::Ajustes& x, const situa::Ajustes& y)
{
    return x.lowCut == y.lowCut && x.highCut == y.highCut && x.drive == y.drive && x.codec == y.codec && x.comp == y.comp
        && x.peakFreq == y.peakFreq && x.peakGain == y.peakGain && x.peakQ == y.peakQ;
}

bool SituaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto entrada = layouts.getMainInputChannelSet(), saida = layouts.getMainOutputChannelSet();
    return entrada == saida && (saida == AudioChannelSet::mono() || saida == AudioChannelSet::stereo());
}

double SituaProcessor::getTailLengthSeconds() const
{
    return pSpace->load() > 0.5f ? pDecay->load() * 1.3 + 0.3 : 0.1;
}

void SituaProcessor::atualizarAtraso (int codec)
{
    const int atraso = situa::Aparelho::atraso (codec, taxa);
    if (atraso != atrasoAtual.load())
    {
        atrasoAtual.store (atraso);
        setLatencySamples (atraso);
    }
}

void SituaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    taxa = sampleRate;
    for (auto& a : aparelhos)
        a.preparar (sampleRate);
    convolucao.preparar (2);
    seco.setSize (2, jmax (samplesPerBlock, 8192));
    linhaSeca.setSize (2, 2048 + jmax (samplesPerBlock, 8192));
    linhaSeca.clear();
    posSeca = 0;
    ganhoSaida.reset (sampleRate, 0.05);
    mixSuave.reset (sampleRate, 0.05);
    aplicarAjustesAgora();
    ganhoSaida.setCurrentAndTargetValue (Decibels::decibelsToGain (pOutput->load()));
    mixSuave.setCurrentAndTargetValue (pMix->load() / 100.0f);
}

void SituaProcessor::aplicarAjustesAgora()
{
    const auto a = lerAjustes();
    ajustesAparelho = a;
    for (auto& ap : aparelhos)
    {
        ap.ajustar (a);
        ap.zerar();
    }
    {
        const ScopedLock l (travaResposta);
        ajustesResposta = a;
        versaoResposta.fetch_add (1);
        convolucao.definirJa (situa::RespostaPreparada::preparar (situa::gerarResposta (a, taxa)));
    }
    atualizarAtraso (a.device ? a.codec : 0);
}

void SituaProcessor::timerCallback()
{
    convolucao.descartarAntigas();
    const auto a = lerAjustes();
    atualizarAtraso (a.device ? a.codec : 0);
    const ScopedLock l (travaResposta);
    if (! situa::mesmaResposta (a, ajustesResposta))
    {
        ajustesResposta = a;
        gerador->pedir (a, taxa, versaoResposta.fetch_add (1) + 1);
    }
}

void SituaProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals semDenormais;
    const int nc = jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (buffer.getNumSamples() == 0 || nc == 0)
        return;
    // blocos maiores que o preparado (o AudioSuite pode mandar) vão em pedaços, sem alocar
    const int maximo = seco.getNumSamples();
    for (int inicio = 0; inicio < buffer.getNumSamples(); inicio += maximo)
    {
        AudioBuffer<float> pedaco (buffer.getArrayOfWritePointers(), nc, inicio, jmin (maximo, buffer.getNumSamples() - inicio));
        processarPedaco (pedaco, nc);
    }
}

void SituaProcessor::processarPedaco (AudioBuffer<float>& buffer, int nc)
{
    const int n = buffer.getNumSamples();
    const auto a = lerAjustes();

    // no render do AudioSuite (fora do tempo real) dá para esperar a resposta nova
    if (isNonRealtime())
    {
        const ScopedLock l (travaResposta);
        if (! situa::mesmaResposta (a, ajustesResposta))
        {
            ajustesResposta = a;
            versaoResposta.fetch_add (1);
            convolucao.definirJa (situa::RespostaPreparada::preparar (situa::gerarResposta (a, taxa)));
        }
    }
    if (! mesmoAparelho (a, ajustesAparelho))
    {
        ajustesAparelho = a;
        for (auto& ap : aparelhos)
            ap.ajustar (a);
    }

    if (nc == 2 && pMono->load() > 0.5f)
    {
        buffer.addFrom (0, 0, buffer, 1, 0, n);
        buffer.applyGain (0, 0, n, 0.5f);
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    }

    // parte seca, atrasada o mesmo que o codec (para o MIX e o BYPASS)
    const int atraso = jmin (atrasoAtual.load(), linhaSeca.getNumSamples() - n - 1);
    const int L = linhaSeca.getNumSamples();
    for (int c = 0; c < nc; ++c)
    {
        auto* linha = linhaSeca.getWritePointer (c);
        const auto* x = buffer.getReadPointer (c);
        auto* s = seco.getWritePointer (c);
        int p = posSeca;
        for (int i = 0; i < n; ++i)
        {
            linha[p] = x[i];
            s[i] = linha[(p - atraso + L) % L];
            p = (p + 1) % L;
        }
    }
    posSeca = (posSeca + n) % L;

    if (pBypass->load() > 0.5f)
    {
        for (int c = 0; c < nc; ++c)
            buffer.copyFrom (c, 0, seco, c, 0, n);
        return;
    }

    if (a.device)
        for (int c = 0; c < nc; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                x[i] = aparelhos[(size_t) c].processar (x[i]);
        }

    convolucao.processar (buffer, nc);

    ganhoSaida.setTargetValue (Decibels::decibelsToGain (pOutput->load()));
    mixSuave.setTargetValue (pMix->load() / 100.0f);
    for (int i = 0; i < n; ++i)
    {
        const float g = ganhoSaida.getNextValue(), m = mixSuave.getNextValue();
        for (int c = 0; c < nc; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            x[i] = x[i] * g * m + seco.getSample (c, i) * (1.0f - m);
        }
    }
}

AudioProcessorEditor* SituaProcessor::createEditor()
{
#if SITUA_SEM_JANELA
    return nullptr;
#else
    return new SituaEditor (*this);
#endif
}

void SituaProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SituaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SituaProcessor();
}
