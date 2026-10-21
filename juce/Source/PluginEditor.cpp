#include "PluginEditor.h"

#include "BinaryData.h"

using namespace juce;

static const char* const idsSliders[] = { ID::lowCut, ID::highCut, ID::drive, ID::muffle, ID::leak, ID::size, ID::decay,
                                          ID::distance, ID::mix, ID::output, ID::lf, ID::hf, ID::peakFreq, ID::peakGain,
                                          ID::peakQ };
static const char* const idsChaves[] = { ID::device, ID::space, ID::barrier, ID::airPlus, ID::mono, ID::bypass, ID::comp,
                                         ID::cabin };
static const char* const idsListas[] = { ID::codec, ID::preset };

SituaEditor::SituaEditor (SituaProcessor& p)
    : AudioProcessorEditor (&p), processador (p)
{
    for (auto* id : idsSliders)
        sliders.push_back (std::make_unique<WebSliderRelay> (id));
    for (auto* id : idsChaves)
        chaves.push_back (std::make_unique<WebToggleButtonRelay> (id));
    for (auto* id : idsListas)
        listas.push_back (std::make_unique<WebComboBoxRelay> (id));

    pagina = std::make_unique<WebBrowserComponent> (opcoes());

    for (size_t i = 0; i < sliders.size(); ++i)
        ligacoesSliders.push_back (std::make_unique<WebSliderParameterAttachment> (
            *processador.state.getParameter (idsSliders[i]), *sliders[i], nullptr));
    for (size_t i = 0; i < chaves.size(); ++i)
        ligacoesChaves.push_back (std::make_unique<WebToggleButtonParameterAttachment> (
            *processador.state.getParameter (idsChaves[i]), *chaves[i], nullptr));
    for (size_t i = 0; i < listas.size(); ++i)
        ligacoesListas.push_back (std::make_unique<WebComboBoxParameterAttachment> (
            *processador.state.getParameter (idsListas[i]), *listas[i], nullptr));

    addAndMakeVisible (*pagina);
    pagina->goToURL (WebBrowserComponent::getResourceProviderRoot());

    setSize (860, 540);
}

WebBrowserComponent::Options SituaEditor::opcoes()
{
    auto o = WebBrowserComponent::Options {}
                 .withBackend (WebBrowserComponent::Options::Backend::webview2)
                 .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2 {}
                                              .withUserDataFolder (File::getSpecialLocation (File::SpecialLocationType::tempDirectory)))
                 .withNativeIntegrationEnabled()
                 .withKeepPageLoadedWhenBrowserIsHidden()
                 .withResourceProvider ([this] (const auto& url) { return recurso (url); })
                 // a página avisa que carregou; com SITUA_TESTE_JANELA=<arquivo> isso vira um
                 // arquivo de conferência (os testes não podem capturar a tela)
                 .withNativeFunction ("janelaPronta", [] (const Array<var>& args, auto completar)
                 {
                     const auto arquivo = SystemStats::getEnvironmentVariable ("SITUA_TESTE_JANELA", {});
                     if (arquivo.isNotEmpty())
                         File (arquivo).replaceWithText (args.isEmpty() ? String() : args[0].toString());
                     completar (var());
                 })
                 // nos testes, SITUA_TESTE_PRESET=<nome> faz a janela carregar esse preset
                 .withNativeFunction ("testePreset", [] (const Array<var>&, auto completar)
                 {
                     completar (var (SystemStats::getEnvironmentVariable ("SITUA_TESTE_PRESET", {})));
                 })
                 .withNativeFunction ("taxa", [this] (const Array<var>&, auto completar)
                 {
                     completar (var (processador.getSampleRate()));
                 });
    for (auto& s : sliders)
        o = o.withOptionsFrom (*s);
    for (auto& c : chaves)
        o = o.withOptionsFrom (*c);
    for (auto& l : listas)
        o = o.withOptionsFrom (*l);
    return o;
}

std::optional<WebBrowserComponent::Resource> SituaEditor::recurso (const String& url)
{
    const auto nome = url == "/" ? String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);

    struct Arquivo { const char* url; const char* dados; int tamanho; const char* mime; };
    const Arquivo arquivos[] = {
        { "index.html", BinaryData::index_html, BinaryData::index_htmlSize, "text/html" },
        { "style.css", BinaryData::style_css, BinaryData::style_cssSize, "text/css" },
        { "app.js", BinaryData::app_js, BinaryData::app_jsSize, "text/javascript" },
        { "fonts/archivo.woff2", BinaryData::archivo_woff2, BinaryData::archivo_woff2Size, "font/woff2" },
    };
    // o juce.js do JUCE é um módulo (export); servimos como script comum que expõe window.Juce
    // (o WebKit do macOS não carrega módulos pelo endereço do plugin)
    if (nome == "juce.js")
    {
        auto codigo = String::fromUTF8 (BinaryData::index_js, BinaryData::index_jsSize)
                          .replace ("import \"./check_native_interop.js\";", "")
                          .replace ("export {", "window.Juce = {");
        codigo = "if (window.__JUCE__) {\n" + codigo + "\n}\n";
        const auto utf8 = codigo.toUTF8();
        const auto* d = reinterpret_cast<const std::byte*> (utf8.getAddress());
        return WebBrowserComponent::Resource { std::vector<std::byte> (d, d + utf8.sizeInBytes() - 1), String ("text/javascript") };
    }
    for (const auto& a : arquivos)
        if (nome == a.url)
            return WebBrowserComponent::Resource { std::vector<std::byte> ((const std::byte*) a.dados, (const std::byte*) a.dados + a.tamanho),
                                                   String (a.mime) };
    return std::nullopt;
}

void SituaEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff1d1c1b));
}

void SituaEditor::resized()
{
    pagina->setBounds (getLocalBounds());
}
