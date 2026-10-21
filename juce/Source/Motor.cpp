#include "Motor.h"

#if SITUA_COM_OPUS
 #include <opus.h>
#endif

#include <algorithm>
#include <cmath>
#include <complex>

namespace situa
{

static constexpr double C = 343.0;
static constexpr double pi = 3.14159265358979323846;

static double db2g (double db) { return std::pow (10.0, db / 20.0); }

// ---------------------------------------------------------------------------------------------
// filtros

void Biquad::definir (double nb0, double nb1, double nb2, double na0, double na1, double na2)
{
    b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
}

void Biquad::passaAltas (double f, double q, double taxa)
{
    const double w = 2 * pi * std::min (f, 0.45 * taxa) / taxa, c = std::cos (w), al = std::sin (w) / (2 * q);
    definir ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}

void Biquad::passaBaixas (double f, double q, double taxa)
{
    const double w = 2 * pi * std::min (f, 0.45 * taxa) / taxa, c = std::cos (w), al = std::sin (w) / (2 * q);
    definir ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}

void Biquad::prateleiraAgudos (double f, double ganhoDb, double q, double taxa)
{
    const double A = std::pow (10.0, ganhoDb / 40), w = 2 * pi * std::min (f, 0.45 * taxa) / taxa;
    const double c = std::cos (w), al = std::sin (w) / (2 * q), r = 2 * std::sqrt (A) * al;
    definir (A * ((A + 1) + (A - 1) * c + r), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - r),
             (A + 1) - (A - 1) * c + r, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - r);
}

void Biquad::sino (double f, double ganhoDb, double q, double taxa)
{
    const double a = std::pow (10.0, ganhoDb / 40), w = 2 * pi * std::min (f, 0.45 * taxa) / taxa;
    const double c = std::cos (w), al = std::sin (w) / (2 * q);
    definir (1 + al * a, -2 * c, 1 - al * a, 1 + al / a, -2 * c, 1 - al / a);
}

// Butterworth: Q de cada seção (4ª e 8ª ordem)
static constexpr double q4[2] = { 0.5412, 1.3066 };
static constexpr double q8[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };

// magnitude² de um Butterworth de ordem n (para as estimativas de nível)
static double butter2 (double f, double fc, int n, bool altas)
{
    const double r = altas ? fc / f : f / fc;
    return 1.0 / (1.0 + std::pow (r, 2 * n));
}

// curva K da loudness (ITU-R BS.1770: prateleira de +4 dB nos agudos e corte abaixo de ~40 Hz), em potência
static double curvaK (double f)
{
    const double r = f / 1682.0;
    return (1.0 + 1.5849 * r * r * r * r) / (1.0 + r * r * r * r) / (1.0 + std::pow (38.0 / f, 4));
}

// peso do espectro médio da fala (potência), ouvido como a loudness ouve. Ajustado nas duas falas de
// teste do protótipo para dar a mesma compensação que a medição de LUFS (erro médio de 0,6 dB)
static double pesoFala (double f)
{
    return curvaK (f) / (1.0 + std::pow (130.0 / f, 4)) / (1.0 + std::pow (f / 250.0, 3));
}

// ganho que devolve à fala o volume que uma curva (potência, função de f) tirou
template <typename Curva>
static double compensarFala (Curva&& curva2)
{
    double antes = 0, depois = 0;
    for (int i = 0; i < 160; ++i)
    {
        const double f = 40.0 * std::pow (12000.0 / 40.0, i / 159.0);
        const double w = pesoFala (f) * f;   // passos em escala log: df ∝ f
        antes += w;
        depois += w * curva2 (f);
    }
    return depois > 1e-20 ? std::sqrt (antes / depois) : 1.0;
}

// ---------------------------------------------------------------------------------------------
// DEVICE

Aparelho::Aparelho()
{
    entradaQuadro.assign (960, 0.0f);
    linhaReferencia.assign (2048, 0.0f);
    saidaQuadro.assign (960, 0.0f);
    quadroDecodificado.assign (960, 0.0f);
}

Aparelho::~Aparelho()
{
#if SITUA_COM_OPUS
    if (codificador != nullptr) opus_encoder_destroy (codificador);
    if (decodificador != nullptr) opus_decoder_destroy (decodificador);
#endif
}

int Aparelho::atraso (int codec, double taxa)
{
#if SITUA_COM_OPUS
    if (codec >= codec2G && std::abs (taxa - 48000.0) < 1.0)
    {
        static const int lookahead = []
        {
            int erro = 0, la = 0;
            auto* e = opus_encoder_create (48000, 1, OPUS_APPLICATION_VOIP, &erro);
            if (e == nullptr) return 312;
            opus_encoder_ctl (e, OPUS_GET_LOOKAHEAD (&la));
            opus_encoder_destroy (e);
            return la;
        }();
        return 960 + lookahead;
    }
#endif
    juce::ignoreUnused (codec, taxa);
    return 0;
}

void Aparelho::preparar (double t)
{
    taxa = t;
    coefComp = std::exp (-1.0 / (0.015 * taxa));
    quedaPico = std::exp (-1.0 / (2.0 * taxa));
    coefNivel = std::exp (-1.0 / (0.4 * taxa));
    coefSuave = std::exp (-1.0 / (0.15 * taxa));
    // curva K da loudness, para o seguidor de nível ouvir como o protótipo (que igualava LUFS)
    for (auto* k : { &kEntrada[0], &kSaida[0] })
        k->prateleiraAgudos (1682.0, 4.0, 0.7071, taxa);
    for (auto* k : { &kEntrada[1], &kSaida[1] })
        k->passaAltas (38.0, 0.5, taxa);
    for (int k = 0; k < 2; ++k)
    {
        antiAlias[k].passaBaixas (3800, q8[k], taxa);
        reconstrucao[k].passaBaixas (3800, q8[k + 2], taxa);
    }
#if SITUA_COM_OPUS
    int erro = 0;
    if (codificador == nullptr && std::abs (taxa - 48000.0) < 1.0)
    {
        codificador = opus_encoder_create (48000, 1, OPUS_APPLICATION_VOIP, &erro);
        decodificador = opus_decoder_create (48000, 1, &erro);
    }
#endif
    codecConfigurado = -1;
    zerar();
}

void Aparelho::zerar()
{
    for (auto* b : { &hp[0], &hp[1], &lp[0], &lp[1], &pico, &posLp, &antiAlias[0], &antiAlias[1], &reconstrucao[0], &reconstrucao[1],
                     &kEntrada[0], &kEntrada[1], &kSaida[0], &kSaida[1] })
        b->zerar();
    envComp = picoSeguido = envEntrada = envSaida = 0.0;
    ganhoNivel = 1.0;
    faseRetencao = 0.0;
    retido = 0.0f;
    std::fill (saidaQuadro.begin(), saidaQuadro.end(), 0.0f);
    posEntrada = posSaida = 0;
    perdidosSeguidos = 0;
    std::fill (linhaReferencia.begin(), linhaReferencia.end(), 0.0f);
    posReferencia = 0;
    sorteio.seed (7);
#if SITUA_COM_OPUS
    if (codificador != nullptr) opus_encoder_ctl (codificador, OPUS_RESET_STATE);
    if (decodificador != nullptr) opus_decoder_ctl (decodificador, OPUS_RESET_STATE);
#endif
}

void Aparelho::ajustar (const Ajustes& a)
{
    aj = a;
    usaHp = a.lowCut > 21.0f;
    usaLp = a.highCut < 19800.0f;
    usaPico = std::abs (a.peakGain) > 0.05f;
    for (int k = 0; k < 2; ++k)
    {
        hp[k].passaAltas (a.lowCut, q4[k], taxa);
        lp[k].passaBaixas (a.highCut, q4[k], taxa);
    }
    pico.sino (a.peakFreq, a.peakGain, a.peakQ, taxa);
    posLp.passaBaixas (a.highCut * 1.1, 0.7071, taxa);

    // estimativa do volume que os filtros tiram (o resto o seguidor de nível acerta)
    const bool opus = a.codec >= codec2G && Aparelho::atraso (a.codec, taxa) > 0;
    ganhoEstatico = compensarFala ([&] (double f)
    {
        double g = 1.0;
        if (usaHp) g *= butter2 (f, a.lowCut, 4, true);
        if (usaLp) g *= butter2 (f, a.highCut, 4, false) * butter2 (f, a.highCut * 1.1, 2, false);
        if (a.codec == codecLandline || (a.codec >= codec2G && ! opus)) g *= butter2 (f, 3800, 8, false);
        if (opus) g *= butter2 (f, (a.codec == codec2G || a.codec == codec3G) ? 4000 : 7500, 8, false);
        if (usaPico)
        {
            const double x = std::log2 (f / a.peakFreq) * a.peakQ * 1.4;
            g *= std::pow (10.0, a.peakGain * std::exp (-x * x) / 10.0);
        }
        return g;
    });

#if SITUA_COM_OPUS
    if (codificador != nullptr && a.codec != codecConfigurado && a.codec >= codec2G)
    {
        const bool estreito = a.codec == codec2G || a.codec == codec3G;
        const int kbps = a.codec == codec2G ? 6000 : a.codec == codec3G ? 8000 : a.codec == codecHD ? 16000 : 12000;
        opus_encoder_ctl (codificador, OPUS_RESET_STATE);
        opus_decoder_ctl (decodificador, OPUS_RESET_STATE);
        opus_encoder_ctl (codificador, OPUS_SET_BITRATE (kbps));
        opus_encoder_ctl (codificador, OPUS_SET_BANDWIDTH (estreito ? OPUS_BANDWIDTH_NARROWBAND : OPUS_BANDWIDTH_WIDEBAND));
        opus_encoder_ctl (codificador, OPUS_SET_MAX_BANDWIDTH (estreito ? OPUS_BANDWIDTH_NARROWBAND : OPUS_BANDWIDTH_WIDEBAND));
        opus_encoder_ctl (codificador, OPUS_SET_SIGNAL (OPUS_SIGNAL_VOICE));
        opus_encoder_ctl (codificador, OPUS_SET_COMPLEXITY (10));
        std::fill (saidaQuadro.begin(), saidaQuadro.end(), 0.0f);
        posEntrada = posSaida = 0;
    }
#endif
    codecConfigurado = a.codec;
    atrasoCodec = std::min (Aparelho::atraso (a.codec, taxa), (int) linhaReferencia.size() - 1);
}

// A-law de 8 bits (G.711, o do Brasil e da Europa)
static float alaw (float x)
{
    constexpr float A = 87.6f, k = 1.0f + 4.4727f;   // 1 + ln(A)
    const float ax = std::min (1.0f, std::abs (x));
    float y = ax < 1.0f / A ? A * ax / k : (1.0f + std::log (A * ax)) / k;
    y = std::round (y * 127.0f) / 127.0f;
    const float sai = y < 1.0f / k ? y * k / A : std::exp (y * k - 1.0f) / A;
    return std::copysign (sai, x);
}

float Aparelho::landline (float x) noexcept
{
    x = antiAlias[1].processar (antiAlias[0].processar (x));
    faseRetencao += 8000.0 / taxa;
    if (faseRetencao >= 1.0)
    {
        faseRetencao -= 1.0;
        retido = alaw (x);
    }
    return reconstrucao[1].processar (reconstrucao[0].processar (retido));
}

float Aparelho::codecAmostra (float x) noexcept
{
    if (aj.codec == codecOff)
        return x;
#if SITUA_COM_OPUS
    if (aj.codec >= codec2G && codificador != nullptr)
    {
        const float sai = saidaQuadro[(size_t) posSaida++];
        entradaQuadro[(size_t) posEntrada++] = x;
        if (posEntrada == 960)
        {
            unsigned char pacote[512];
            const int bytes = opus_encode_float (codificador, entradaQuadro.data(), 960, pacote, (opus_int32) sizeof (pacote));
            bool perde = false;
            if (aj.codec == codecVoip)
            {
                // perda de pacotes em rajadas de 1 a 3 quadros (4% dos quadros começam uma)
                if (perdidosSeguidos > 0) { perde = true; --perdidosSeguidos; }
                else if (std::uniform_real_distribution<float> (0.0f, 1.0f) (sorteio) < 0.04f)
                {
                    perdidosSeguidos = std::uniform_int_distribution<int> (1, 3) (sorteio) - 1;
                    perde = true;
                }
            }
            const int n = (perde || bytes <= 0) ? opus_decode_float (decodificador, nullptr, 0, quadroDecodificado.data(), 960, 0)
                                                : opus_decode_float (decodificador, pacote, bytes, quadroDecodificado.data(), 960, 0);
            for (int i = 0; i < 960; ++i)
                saidaQuadro[(size_t) i] = i < n ? quadroDecodificado[(size_t) i] : 0.0f;
            posEntrada = posSaida = 0;
        }
        return sai;
    }
#endif
    return landline (x);   // LANDLINE, ou os outros quando não há Opus (taxa ≠ 48 kHz)
}

static float spiral (float x)
{
    // Spiral do Airwindows (MIT, Chris Johnson): sin(x·|x|)/|x|
    x = std::clamp (x, -1.2533f, 1.2533f);
    const float ax = std::abs (x);
    return ax > 1e-9f ? std::sin (x * ax) / ax : x;
}

float Aparelho::processar (float x) noexcept
{
    const float entrada = x;
    if (usaHp) x = hp[1].processar (hp[0].processar (x));
    if (usaLp) x = lp[1].processar (lp[0].processar (x));
    if (usaPico) x = pico.processar (x);
    if (aj.comp)
    {
        envComp = coefComp * envComp + (1 - coefComp) * (double) x * x;
        const double nivel = 10 * std::log10 (envComp + 1e-12);
        const double reducao = std::min (0.0, (-24.0 - nivel) * (1 - 1 / 4.0));
        x = (float) (x * db2g (reducao));
    }
    if (aj.drive > 0.0f)
    {
        picoSeguido = std::max ((double) std::abs (x), picoSeguido * quedaPico);
        const double d = aj.drive / 100.0;
        const double g = (0.25 + 4.0 * std::pow (d, 1.5)) / std::max (picoSeguido, 0.003);
        // dividir pelo ganho mantém o volume do que não satura (só os picos achatam)
        x = (float) (spiral ((float) (x * g)) / g);
    }
    x = codecAmostra (x);
    if (usaLp) x = posLp.processar (x);   // o alto-falante do aparelho corta de novo depois do codec

    // nível: a saída segue o volume da entrada (o protótipo igualava a loudness do arquivo)
    const double y = x * ganhoEstatico;
    // a entrada de referência vai atrasada o mesmo que o codec, senão o começo de cada frase
    // (saída ainda muda, entrada já falando) puxa o ganho para cima
    const float keAgora = kEntrada[1].processar (kEntrada[0].processar (entrada));
    linhaReferencia[(size_t) posReferencia] = keAgora;
    const int nRef = (int) linhaReferencia.size();
    const double ke = linhaReferencia[(size_t) ((posReferencia - atrasoCodec + nRef) % nRef)];
    posReferencia = (posReferencia + 1) % nRef;
    const double ks = kSaida[1].processar (kSaida[0].processar ((float) y));
    envEntrada = coefNivel * envEntrada + (1 - coefNivel) * ke * ke;
    envSaida = coefNivel * envSaida + (1 - coefNivel) * ks * ks;
    if (envEntrada > 1e-6 && envSaida > 1e-12)   // acima de −60 dB: nas pausas o ganho fica parado
    {
        const double alvo = std::clamp (std::sqrt (envEntrada / envSaida), 1.0 / 16.0, 16.0);
        ganhoNivel = coefSuave * ganhoNivel + (1 - coefSuave) * alvo;
    }
    return (float) (y * ganhoNivel);
}

// ---------------------------------------------------------------------------------------------
// SPACE + BARRIER: a resposta ao impulso

struct Dimensoes { double L, W, H; };

static Dimensoes dimensoes (double tam)
{
    const double L = std::min (tam, 60.0);
    const double H = L < 12 ? std::min (0.45 * L, 2.7) : 0.45 * L;
    return { L, 0.75 * L, H };
}

float drAlvo (const Ajustes& a)
{
    const double d = a.distance;
    if (a.size >= 60)
        return (float) (8 - 10 * std::log10 (d / 2));
    const auto s = dimensoes (a.size);
    const double dc = 0.057 * std::sqrt (s.L * s.W * s.H / a.decay);
    return (float) std::clamp (20 * std::log10 (dc / d) + 10, -15.0, 15.0);
}

bool mesmaResposta (const Ajustes& x, const Ajustes& y)
{
    if (x.space != y.space || x.barrier != y.barrier)
        return false;
    if (x.space && (x.size != y.size || x.decay != y.decay || x.distance != y.distance || x.lf != y.lf
                    || x.hf != y.hf || x.airPlus != y.airPlus || x.cabin != y.cabin))
        return false;
    if (x.barrier && (x.muffle != y.muffle || x.leak != y.leak))
        return false;
    return true;
}

// absorção do ar, ISO 9613-1 (20 °C, 50% UR), em dB/m
static double arIso (double f)
{
    const double T = 293.15, h = 1.1645;
    const double fO = 24 + 4.04e4 * h * (0.02 + h) / (0.391 + h), fN = 9 + 280 * h;
    return 8.686 * f * f * (1.84e-11 + 0.01275 * std::exp (-2239.1 / T) / (fO + f * f / fO)
                            + 0.1068 * std::exp (-3352 / T) / (fN + f * f / fN));
}

static double cantoParede (double m) { return 1500 * std::pow (100.0 / 1500.0, m); }

// parede pela lei da massa (só a forma; o nível é tratado à parte), em amplitude
static double parede (const Ajustes& a, double f)
{
    const double m = a.muffle / 100.0, fc = cantoParede (m), ordem = 1 + m;
    const double vaza = db2g (-40 + 28 * a.leak / 100.0);
    const double p = db2g (-ordem * 10 * std::log10 (1 + (f / fc) * (f / fc)) - (4 + 10 * m));
    const double fresta = vaza / std::sqrt (1 + (1000 / f) * (1000 / f));
    return p + fresta;
}

// curva da parte linear em dB (ar, proximidade, cabine, AIR+ e parede)
static double curvaDb (const Ajustes& a, double f)
{
    double c = 0;
    if (a.space)
    {
        const double d = a.distance;
        c -= arIso (f) * d * (a.airPlus ? 4 : 1);
        c -= std::min (1.0, d / 6) * 4 * (150.0 * 150.0 / (f * f + 150.0 * 150.0));
        if (a.cabin) c += 4 * (120.0 * 120.0 / (f * f + 120.0 * 120.0));
        if (a.airPlus) c -= 6 * (f * f / (f * f + 3000.0 * 3000.0));
    }
    if (a.barrier)
        c += 20 * std::log10 (parede (a, f));
    return c;
}

// FFT real de qualquer tamanho potência de 2, com a escala da inversa medida
struct FFTReal
{
    explicit FFTReal (int ordem) : fft (ordem), n (1 << ordem)
    {
        std::vector<float> t ((size_t) (2 * n), 0.0f);
        t[0] = 1.0f;
        fft.performRealOnlyForwardTransform (t.data(), true);
        fft.performRealOnlyInverseTransform (t.data());
        escala = std::abs (t[0]) > 1e-12f ? 1.0f / t[0] : 1.0f;
    }
    juce::dsp::FFT fft;
    int n;
    float escala = 1.0f;
};

static int ordemPara (int amostras)
{
    int o = 1;
    while ((1 << o) < amostras) ++o;
    return o;
}

// filtro de fase mínima com a magnitude dada (método do cepstro)
static std::vector<float> fasesMinima (const Ajustes& a, double taxa, int taps)
{
    const int ordem = 14, K = 1 << ordem;
    juce::dsp::FFT fft (ordem);
    using Cx = juce::dsp::Complex<float>;
    std::vector<Cx> x ((size_t) K), y ((size_t) K);

    // escala da inversa complexa
    std::fill (x.begin(), x.end(), Cx (0, 0));
    x[0] = Cx (1, 0);
    fft.perform (x.data(), y.data(), false);
    fft.perform (y.data(), x.data(), true);
    const float esc = std::abs (x[0].real()) > 1e-12f ? 1.0f / x[0].real() : 1.0f;

    for (int k = 0; k <= K / 2; ++k)
    {
        const double f = std::max (1.0, k * taxa / K);
        const float lm = (float) (curvaDb (a, f) / 20.0 * std::log (10.0));
        x[(size_t) k] = Cx (std::max (lm, -16.0f), 0);   // −16 neper ≈ −139 dB de piso
        if (k > 0 && k < K / 2)
            x[(size_t) (K - k)] = x[(size_t) k];
    }
    fft.perform (x.data(), y.data(), true);   // cepstro
    for (int k = 0; k < K; ++k)
    {
        const float c = y[(size_t) k].real() * esc;
        const float dobra = (k == 0 || k == K / 2) ? 1.0f : (k < K / 2 ? 2.0f : 0.0f);
        x[(size_t) k] = Cx (c * dobra, 0);
    }
    fft.perform (x.data(), y.data(), false);
    for (int k = 0; k < K; ++k)
        x[(size_t) k] = std::exp (y[(size_t) k]);
    fft.perform (x.data(), y.data(), true);

    std::vector<float> h ((size_t) taps);
    const int fade = taps / 8;
    for (int i = 0; i < taps; ++i)
    {
        float w = 1.0f;
        if (i >= taps - fade)
            w = 0.5f * (1.0f + std::cos ((float) pi * (float) (i - (taps - fade)) / (float) fade));
        h[(size_t) i] = y[(size_t) i].real() * esc * w;
    }
    return h;
}

static std::vector<float> convolver (const std::vector<float>& a, const std::vector<float>& b)
{
    const int n = (int) (a.size() + b.size() - 1);
    FFTReal f (ordemPara (n));
    std::vector<float> x ((size_t) (2 * f.n), 0.0f), y ((size_t) (2 * f.n), 0.0f);
    std::copy (a.begin(), a.end(), x.begin());
    std::copy (b.begin(), b.end(), y.begin());
    f.fft.performRealOnlyForwardTransform (x.data(), true);
    f.fft.performRealOnlyForwardTransform (y.data(), true);
    for (int k = 0; k <= f.n / 2; ++k)
    {
        const float xr = x[(size_t) (2 * k)], xi = x[(size_t) (2 * k + 1)], yr = y[(size_t) (2 * k)], yi = y[(size_t) (2 * k + 1)];
        x[(size_t) (2 * k)] = xr * yr - xi * yi;
        x[(size_t) (2 * k + 1)] = xr * yi + xi * yr;
    }
    f.fft.performRealOnlyInverseTransform (x.data());
    std::vector<float> r ((size_t) n);
    for (int i = 0; i < n; ++i)
        r[(size_t) i] = x[(size_t) i] * f.escala;
    return r;
}

// reflexões iniciais + cauda + som direto (o mesmo de resposta_impulso() no protótipo)
static std::vector<float> sala (const Ajustes& a, double taxa)
{
    std::mt19937 gerador (3);
    std::normal_distribution<float> normal;
    const double d = a.distance, rt = a.decay;
    const bool aberto = a.size >= 60;
    const int n = (int) ((rt * 1.3 + 0.25) * taxa);
    std::vector<double> er ((size_t) n, 0.0);
    double mistura = 0.01;

    if (aberto)
    {
        // chão e duas fachadas (12 m atrás, 20 m ao lado); voz e ouvinte a 1,6 m de altura
        const double caminhos[4][2] = { { std::hypot (d, 3.2), 0.7 }, { d + 24.0, 0.5 }, { std::hypot (d, 40.0), 0.4 }, { d + 64.0, 0.2 } };
        for (auto& c : caminhos)
        {
            const int t = (int) ((c[0] - d) / C * taxa);
            if (t < n) er[(size_t) t] += c[1] * d / c[0];
        }
    }
    else
    {
        const auto s = dimensoes (a.size);
        const double V = s.L * s.W * s.H, S = 2 * (s.L * s.W + s.L * s.H + s.W * s.H);
        const double alfa = std::clamp (0.161 * V / (S * rt), 0.02, 0.95), beta = std::sqrt (1 - alfa);
        // voz e ouvinte simétricos ao centro, pela diagonal do piso
        const double h = std::min (1.6, s.H * 0.6);
        const double ex = s.L / std::hypot (s.L, s.W * 0.6), ey = s.W * 0.6 / std::hypot (s.L, s.W * 0.6);
        const double meio = std::min (d / 2, std::min ((s.L / 2 - 0.3) / ex, (s.W / 2 - 0.3) / ey));
        const double fonte[3] = { s.L / 2 + ex * meio, s.W / 2 + ey * meio, h };
        const double ouv[3] = { s.L / 2 - ex * meio, s.W / 2 - ey * meio, h };
        const double dd = std::max (std::hypot (fonte[0] - ouv[0], fonte[1] - ouv[1]), 0.3);
        const double dims[3] = { s.L, s.W, s.H };
        const int ordem = 3;
        for (int nx = -ordem; nx <= ordem; ++nx)
            for (int ny = -ordem; ny <= ordem; ++ny)
                for (int nz = -ordem; nz <= ordem; ++nz)
                    for (int px = 0; px < 2; ++px)
                        for (int py = 0; py < 2; ++py)
                            for (int pz = 0; pz < 2; ++pz)
                            {
                                const int k = std::abs (2 * nx - px) + std::abs (2 * ny - py) + std::abs (2 * nz - pz);
                                if (k == 0 || k > ordem) continue;
                                const int nn[3] = { nx, ny, nz }, pp[3] = { px, py, pz };
                                double r2 = 0;
                                for (int e = 0; e < 3; ++e)
                                {
                                    const double img = (1 - 2 * pp[e]) * fonte[e] + 2 * nn[e] * dims[e];
                                    r2 += (img - ouv[e]) * (img - ouv[e]);
                                }
                                const double r = std::sqrt (r2);
                                const int t = (int) ((r - dd) / C * taxa);
                                if (t > 0 && t < n) er[(size_t) t] += std::pow (beta, k) * dd / r;
                            }
        mistura = std::min (0.08, 0.002 * std::sqrt (V) + 0.005);
    }

    // cauda: ruído em bandas de oitava, cada banda com seu RT60
    FFTReal f (ordemPara (n));
    std::vector<float> ruido ((size_t) (2 * f.n), 0.0f), banda ((size_t) (2 * f.n));
    for (int i = 0; i < n; ++i) ruido[(size_t) i] = normal (gerador);
    f.fft.performRealOnlyForwardTransform (ruido.data(), true);
    const double bordas[8] = { 20, 250, 500, 1000, 2000, 4000, 8000, 23000 };
    const double mult[7] = { a.lf, (a.lf + 1) / 2, 1.0, 1.0, (1 + a.hf) / 2, a.hf, a.hf * 0.8 };
    std::vector<double> cauda ((size_t) n, 0.0);
    for (int b = 0; b < 7; ++b)
    {
        std::fill (banda.begin(), banda.end(), 0.0f);
        for (int k = 0; k <= f.n / 2; ++k)
        {
            const double fk = k * taxa / f.n;
            if (fk >= bordas[b] && fk < bordas[b + 1])
            {
                banda[(size_t) (2 * k)] = ruido[(size_t) (2 * k)];
                banda[(size_t) (2 * k + 1)] = ruido[(size_t) (2 * k + 1)];
            }
        }
        f.fft.performRealOnlyInverseTransform (banda.data());
        const double tau = std::max (0.03, rt * mult[b]);
        for (int i = 0; i < n; ++i)
            cauda[(size_t) i] += banda[(size_t) i] * f.escala * std::exp (-6.91 * (i / taxa) / tau);
    }
    for (int i = 0; i < n; ++i)
        cauda[(size_t) i] *= std::clamp ((i / taxa - mistura) / 0.01, 0.0, 1.0) * (aberto ? 0.5 : 1.0);

    // as reflexões perdem agudo nas paredes (antes de dosar a energia)
    {
        const double K = std::tan (pi * std::min (6000.0 * a.hf + 1500.0, 0.45 * taxa) / taxa);
        const double b0 = K / (1 + K), a1 = (K - 1) / (K + 1);
        double z = 0;
        for (auto& v : er)
        {
            const double y = b0 * v + z;
            z = b0 * v - a1 * y;
            v = y;
        }
    }
    // energia: reverb total (reflexões + cauda) / direto = −D/R
    const double alvo = std::pow (10.0, -drAlvo (a) / 10.0);
    double eEr = 0, eCauda = 0;
    for (auto v : er) eEr += v * v;
    for (auto v : cauda) eCauda += v * v;
    if (eEr > alvo * 0.6)
    {
        const double g = std::sqrt (alvo * 0.6 / eEr);
        for (auto& v : er) v *= g;
        eEr = alvo * 0.6;
    }
    const double gc = std::sqrt (std::max (alvo - eEr, 1e-9) / std::max (eCauda, 1e-20));
    std::vector<float> ri ((size_t) n);
    for (int i = 0; i < n; ++i)
        ri[(size_t) i] = (float) (er[(size_t) i] + cauda[(size_t) i] * gc);
    ri[0] += 1.0f;   // voz direta sem atraso (sincronia)
    return ri;
}

std::vector<float> gerarResposta (const Ajustes& a, double taxa)
{
    if (! a.space && ! a.barrier)
        return { 1.0f };
    std::vector<float> ri = a.space ? sala (a, taxa) : std::vector<float> { 1.0f };
    auto h = convolver (ri, fasesMinima (a, taxa, 4096));

    double ganho = 1.0;
    if (a.space)
    {
        // a sala soma energia (o reverb); o volume da voz fica o mesmo e só a distância baixa:
        // −3 dB por dobra de distância (metade da física), até −8 dB
        double energia = 0;
        for (auto v : ri) energia += (double) v * v;
        ganho *= db2g (-std::min (8.0, 3 * std::log2 (std::max ((double) a.distance, 1.0)))) / std::sqrt (std::max (energia, 1.0));
    }
    if (a.barrier)
    {
        // a parede muda o timbre; o volume cai só o combinado (−4 a −14 dB)
        const double m = a.muffle / 100.0;
        ganho *= compensarFala ([&] (double f) { const double p = parede (a, f); return p * p; }) * db2g (-(4 + 10 * m));
    }
    for (auto& v : h) v = (float) (v * ganho);
    // tira o rabo de silêncio (abaixo de −100 dB do pico)
    float pico = 0;
    for (auto v : h) pico = std::max (pico, std::abs (v));
    int fim = (int) h.size();
    while (fim > 1 && std::abs (h[(size_t) (fim - 1)]) < pico * 1e-5f) --fim;
    h.resize ((size_t) fim);
    return h;
}

} // namespace situa
