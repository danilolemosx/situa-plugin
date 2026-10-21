#include "Convolucao.h"

namespace situa
{

static constexpr int P = RespostaPreparada::P;
static constexpr int N = 2 * P;            // tamanho da FFT
static constexpr int nBins = P + 1;        // bins não negativos

std::shared_ptr<RespostaPreparada> RespostaPreparada::preparar (const std::vector<float>& h)
{
    auto r = std::make_shared<RespostaPreparada>();
    r->comprimento = (int) h.size();
    r->cabeca.assign (P, 0.0f);
    for (int i = 0; i < juce::jmin (P, (int) h.size()); ++i)
        r->cabeca[(size_t) i] = h[(size_t) i];

    juce::dsp::FFT fft (ordemFFT);
    std::vector<float> buf ((size_t) (2 * N));
    for (int inicio = P; inicio < (int) h.size(); inicio += P)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < P && inicio + i < (int) h.size(); ++i)
            buf[(size_t) i] = h[(size_t) (inicio + i)];
        fft.performRealOnlyForwardTransform (buf.data(), true);
        r->partes.emplace_back (buf.begin(), buf.begin() + 2 * nBins);
    }
    return r;
}

// ---------------------------------------------------------------------------------------------

void CanalConvolucao::iniciar (std::shared_ptr<const RespostaPreparada> r)
{
    resposta = std::move (r);
    fft = std::make_unique<juce::dsp::FFT> (RespostaPreparada::ordemFFT);
    historia.assign ((size_t) (2 * P), 0.0f);
    quadroAnterior.assign (P, 0.0f);
    quadroAtual.assign (P, 0.0f);
    saidaCauda.assign (P, 0.0f);
    trabalho.assign ((size_t) (2 * N), 0.0f);
    soma.assign ((size_t) (2 * nBins), 0.0f);
    linha.assign (resposta->partes.size(), std::vector<float> ((size_t) (2 * nBins), 0.0f));
    posHistoria = posLinha = pos = 0;

    // a escala da FFT inversa muda conforme a implementação: medida uma vez
    std::fill (trabalho.begin(), trabalho.end(), 0.0f);
    trabalho[0] = 1.0f;
    fft->performRealOnlyForwardTransform (trabalho.data(), true);
    fft->performRealOnlyInverseTransform (trabalho.data());
    escalaInversa = std::abs (trabalho[0]) > 1e-12f ? 1.0f / trabalho[0] : 1.0f;
}

float CanalConvolucao::processar (float x) noexcept
{
    // cabeça: convolução direta com h[0..P), história duplicada para ler sem dar a volta
    historia[(size_t) posHistoria] = x;
    historia[(size_t) (posHistoria + P)] = x;
    const float* hist = historia.data() + posHistoria + P;
    const float* cab = resposta->cabeca.data();
    float y = 0.0f;
    for (int k = 0; k < P; ++k)
        y += cab[k] * hist[-k];
    posHistoria = (posHistoria + 1) % P;

    // cauda: a parte calculada no fim do quadro anterior
    quadroAtual[(size_t) pos] = x;
    y += saidaCauda[(size_t) pos];
    if (++pos == P)
    {
        quadro();
        pos = 0;
    }
    return y;
}

void CanalConvolucao::quadro() noexcept
{
    const int M = (int) linha.size();
    if (M > 0)
    {
        std::fill (trabalho.begin(), trabalho.end(), 0.0f);
        std::copy (quadroAnterior.begin(), quadroAnterior.end(), trabalho.begin());
        std::copy (quadroAtual.begin(), quadroAtual.end(), trabalho.begin() + P);
        fft->performRealOnlyForwardTransform (trabalho.data(), true);
        std::copy (trabalho.begin(), trabalho.begin() + 2 * nBins, linha[(size_t) posLinha].begin());

        std::fill (soma.begin(), soma.end(), 0.0f);
        for (int m = 0; m < M; ++m)
        {
            const float* X = linha[(size_t) ((posLinha - m + M) % M)].data();
            const float* H = resposta->partes[(size_t) m].data();
            float* S = soma.data();
            for (int b = 0; b < nBins; ++b)
            {
                const float xr = X[2 * b], xi = X[2 * b + 1], hr = H[2 * b], hi = H[2 * b + 1];
                S[2 * b] += xr * hr - xi * hi;
                S[2 * b + 1] += xr * hi + xi * hr;
            }
        }
        std::fill (trabalho.begin(), trabalho.end(), 0.0f);
        std::copy (soma.begin(), soma.end(), trabalho.begin());
        fft->performRealOnlyInverseTransform (trabalho.data());
        for (int i = 0; i < P; ++i)
            saidaCauda[(size_t) i] = trabalho[(size_t) (P + i)] * escalaInversa;
        posLinha = (posLinha + 1) % M;
    }
    std::swap (quadroAnterior, quadroAtual);
}

// ---------------------------------------------------------------------------------------------

static std::vector<CanalConvolucao> montar (const std::shared_ptr<RespostaPreparada>& r, int canais)
{
    std::vector<CanalConvolucao> v ((size_t) canais);
    for (auto& c : v)
        c.iniciar (r);
    return v;
}

void Convolucao::preparar (int canais)
{
    const juce::SpinLock::ScopedLockType l (trava);
    canaisPreparados = canais;
}

void Convolucao::definirJa (std::shared_ptr<RespostaPreparada> r)
{
    auto v = montar (r, canaisPreparados);
    const juce::SpinLock::ScopedLockType l (trava);
    atual.swap (v);
    entrando.clear();
    pendente.clear();
    temPendente = false;
    restanteCruzamento = 0;
}

void Convolucao::definir (std::shared_ptr<RespostaPreparada> r)
{
    auto v = montar (r, canaisPreparados);   // as alocações ficam fora da thread de áudio
    const juce::SpinLock::ScopedLockType l (trava);
    pendente.swap (v);
    temPendente = true;
}   // a pendente que ninguém usou morre aqui, fora da thread de áudio

void Convolucao::processar (juce::AudioBuffer<float>& audio, int canais) noexcept
{
    if (restanteCruzamento == 0 && ! lixeiraCheia.load (std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedTryLockType l (trava);
        if (l.isLocked() && temPendente)
        {
            entrando.swap (pendente);
            temPendente = false;
            restanteCruzamento = atual.empty() ? 0 : cruzamento;
            if (atual.empty())
                atual.swap (entrando);
        }
    }

    const int n = audio.getNumSamples();
    const int nc = juce::jmin (canais, (int) atual.size());
    if (nc == 0)
        return;
    const int restante0 = restanteCruzamento;
    for (int c = 0; c < nc; ++c)
    {
        float* x = audio.getWritePointer (c);
        auto& a = atual[(size_t) c];
        if (restante0 > 0 && c < (int) entrando.size())
        {
            auto& b = entrando[(size_t) c];
            int restante = restante0;
            for (int i = 0; i < n; ++i)
            {
                const float ya = a.processar (x[i]), yb = b.processar (x[i]);
                const float w = restante > 0 ? 1.0f - (float) restante / (float) cruzamento : 1.0f;
                x[i] = ya * (1.0f - w) + yb * w;
                if (restante > 0)
                    --restante;
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
                x[i] = a.processar (x[i]);
        }
    }
    if (restante0 > 0)
    {
        restanteCruzamento = juce::jmax (0, restante0 - n);
        if (restanteCruzamento == 0)
        {
            atual.swap (entrando);       // a nova vira a atual
            lixeira.swap (entrando);     // a antiga vai para a lixeira (troca sem alocar)
            lixeiraCheia.store (true, std::memory_order_release);
        }
    }
}

void Convolucao::descartarAntigas()
{
    if (lixeiraCheia.load (std::memory_order_acquire))
    {
        std::vector<CanalConvolucao> fora;
        fora.swap (lixeira);
        lixeiraCheia.store (false, std::memory_order_release);
    }
}

} // namespace situa
