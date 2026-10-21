// Testes do Situa fora do Avid.
//
//   testa-situa <entrada.wav> <saida.wav> [param=valor ...] [--bloco N] [--tempo-real]
//       toca o WAV pelo processador em blocos (padrão 512), com a cauda, e grava a saída em
//       float 32 já sem o atraso do plugin. Valores reais dos parâmetros, ex.:
//       space=1 size=5 decay=0.5 distance=2 barrier=1 muffle=70 leak=5
//       (codec: 0 OFF, 1 LANDLINE, 2 2G, 3 3G, 4 HD, 5 VOIP)
//       Sem --tempo-real, roda como o render do AudioSuite (isNonRealtime).
//
//   testa-situa --janela <Situa.vst3|Situa.component> [segundos]
//       abre a janela do plugin por alguns segundos (com SITUA_TESTE_JANELA=<arquivo>,
//       a página grava ali o que desenhou)

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Source/PluginProcessor.h"

#include <iostream>

using namespace juce;

static int falha (const String& msg)
{
    std::cerr << "ERRO: " << msg << std::endl;
    return 1;
}

static int abrirJanela (const File& arquivoPlugin, double segundos)
{
    AudioPluginFormatManager formatos;
    formatos.addFormat (new VST3PluginFormat());
   #if JUCE_PLUGINHOST_AU
    formatos.addFormat (new AudioUnitPluginFormat());
   #endif
    OwnedArray<PluginDescription> descricoes;
    for (auto* f : formatos.getFormats())
    {
        const auto id = f->getName() == "AudioUnit" ? String ("aufx Situ DnLm") : arquivoPlugin.getFullPathName();
        if (f->getName() == "AudioUnit" ? arquivoPlugin.getFileExtension() == ".component" : f->fileMightContainThisPluginType (id))
            f->findAllTypesForFile (descricoes, id);
    }
    if (descricoes.isEmpty())
        return falha ("plugin não encontrado");
    String erro;
    auto plugin = formatos.createPluginInstance (*descricoes[0], 48000.0, 512, erro);
    if (plugin == nullptr)
        return falha ("não consegui abrir o plugin: " + erro);
    std::unique_ptr<AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
    if (editor == nullptr)
        return falha ("o plugin não criou a janela");
    std::cout << "janela " << editor->getWidth() << "x" << editor->getHeight() << std::endl;
    DocumentWindow janela ("Situa (teste)", Colours::black, 0);
    janela.setContentNonOwned (editor.get(), true);
    janela.setTopLeftPosition (80, 80);
    janela.setVisible (true);
    MessageManager::getInstance()->runDispatchLoopUntil ((int) (segundos * 1000));
    // os parâmetros como o plugin ficou (a janela pode ter carregado um preset)
    for (auto* p : plugin->getParameters())
        std::cout << "  " << p->getName (40) << " = " << p->getCurrentValueAsText() << std::endl;
    janela.clearContentComponent();
    editor.reset();
    return 0;
}

static bool gravar (const File& destino, const AudioBuffer<float>& audio, double taxa)
{
    WavAudioFormat wav;
    destino.deleteFile();
    std::unique_ptr<OutputStream> saida (destino.createOutputStream());
    std::unique_ptr<AudioFormatWriter> escritor (wav.createWriterFor (saida.get(), taxa, (unsigned int) audio.getNumChannels(), 32, {}, 0));
    if (escritor == nullptr)
        return false;
    saida.release();
    return escritor->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;
    if (argc >= 3 && String (argv[1]) == "--janela")
        return abrirJanela (File (argv[2]), argc > 3 ? String (argv[3]).getDoubleValue() : 5.0);
    if (argc < 3)
        return falha ("uso: testa-situa <entrada.wav> <saida.wav> [param=valor ...] [--bloco N] [--tempo-real]");

    AudioFormatManager formatos;
    formatos.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> leitor (formatos.createReaderFor (File (argv[1])));
    if (leitor == nullptr)
        return falha ("não consegui ler " + String (argv[1]));
    const int canais = (int) jmin (2u, leitor->numChannels);
    AudioBuffer<float> entrada (canais, (int) leitor->lengthInSamples);
    leitor->read (&entrada, 0, (int) leitor->lengthInSamples, 0, true, canais > 1);

    SituaProcessor situa;
    int bloco = 512;
    bool tempoReal = false;
    for (int i = 3; i < argc; ++i)
    {
        const String arg (argv[i]);
        if (arg == "--bloco" && i + 1 < argc) { bloco = String (argv[++i]).getIntValue(); continue; }
        if (arg == "--tempo-real") { tempoReal = true; continue; }
        const auto id = arg.upToFirstOccurrenceOf ("=", false, false);
        auto* p = situa.state.getParameter (id);
        if (p == nullptr)
            return falha ("parâmetro desconhecido: " + arg);
        p->setValueNotifyingHost (p->convertTo0to1 (arg.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    }
    const auto layout = canais == 1 ? AudioChannelSet::mono() : AudioChannelSet::stereo();
    AudioProcessor::BusesLayout l;
    l.inputBuses.add (layout);
    l.outputBuses.add (layout);
    if (! situa.setBusesLayout (l))
        return falha ("layout não aceito");
    situa.setNonRealtime (! tempoReal);
    const auto inicio = Time::getMillisecondCounterHiRes();
    situa.prepareToPlay (leitor->sampleRate, bloco);
    const auto preparo = Time::getMillisecondCounterHiRes() - inicio;
    const int atraso = situa.getLatencySamples();
    const int cauda = (int) (situa.getTailLengthSeconds() * leitor->sampleRate);
    const int total = entrada.getNumSamples() + cauda;

    AudioBuffer<float> saida (canais, total);
    saida.clear();
    AudioBuffer<float> b (canais, bloco);
    MidiBuffer midi;
    const auto t0 = Time::getMillisecondCounterHiRes();
    for (int i = 0; i < total + atraso; i += bloco)
    {
        b.clear();
        for (int c = 0; c < canais; ++c)
            if (i < entrada.getNumSamples())
                b.copyFrom (c, 0, entrada, c, i, jmin (bloco, entrada.getNumSamples() - i));
        situa.processBlock (b, midi);
        for (int c = 0; c < canais; ++c)
            for (int j = 0; j < bloco; ++j)
            {
                const int destino = i + j - atraso;
                if (destino >= 0 && destino < total)
                    saida.setSample (c, destino, b.getSample (c, j));
            }
    }
    const auto t1 = Time::getMillisecondCounterHiRes();
    if (! gravar (File (argv[2]), saida, leitor->sampleRate))
        return falha ("não consegui gravar " + String (argv[2]));
    const double segundos = total / leitor->sampleRate;
    std::cout << "ok: " << canais << " canal(is), " << String (segundos, 2) << " s, atraso " << atraso << " amostras, cauda "
              << cauda << ", preparo " << String (preparo, 0) << " ms, processamento " << String ((t1 - t0) / 1000.0 / segundos * 100.0, 1)
              << "% do tempo real, pico " << String (Decibels::gainToDecibels (saida.getMagnitude (0, total)), 1) << " dBFS" << std::endl;
    return 0;
}
