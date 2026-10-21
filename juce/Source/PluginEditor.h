// Situa: janela do plugin (página HTML em ui/, ligada aos parâmetros).

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

class SituaEditor : public juce::AudioProcessorEditor
{
public:
    explicit SituaEditor (SituaProcessor&);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    std::optional<juce::WebBrowserComponent::Resource> recurso (const juce::String& url);
    juce::WebBrowserComponent::Options opcoes();

    SituaProcessor& processador;

    // um "relay" por parâmetro (nome = ID do parâmetro, usado pelo app.js)
    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliders;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> chaves;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>> listas;

    std::unique_ptr<juce::WebBrowserComponent> pagina;

    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> ligacoesSliders;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> ligacoesChaves;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> ligacoesListas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SituaEditor)
};
