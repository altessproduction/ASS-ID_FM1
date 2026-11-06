// Referencia: la voz del módulo ATEK303 de VCV (Open303 + AtekOsc + AtekFilter), sin Rack, con los
// valores por defecto del módulo y la deriva apagada. Sale un WAV mono de 32 bits float.
#include "rosic_Open303.h"
#include "AtekOsc.hpp"
#include "AtekFilter.hpp"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
extern "C" {
#include "secuencia.h"
}
static void wav(const char *path, const std::vector<float> &s) {
    FILE *f = fopen(path, "wb"); unsigned n = s.size() * 4, r = 36 + n, sr = SR, br = SR * 4, fs = 16;
    unsigned short fmt = 3, ch = 1, ba = 4, bits = 32;
    fwrite("RIFF",1,4,f); fwrite(&r,4,1,f); fwrite("WAVEfmt ",1,8,f); fwrite(&fs,4,1,f); fwrite(&fmt,2,1,f);
    fwrite(&ch,2,1,f); fwrite(&sr,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&n,4,1,f); fwrite(s.data(),4,s.size(),f); fclose(f);
}
int main(int argc, char **argv) {
    rosic::Open303 core; AtekOsc osc; AtekFilter filt;
    const double sr = SR, DT = 2.302585092994046;
    core.setSampleRate(sr);
    core.externalOscillator = &osc;
    core.externalFilter = &filt; filt.setSampleRate(4.0 * sr); filt.reset();
    filt.setDrive(2.0); core.otaHeadroom = 0.6;
    osc.setShape(0.44, 60.0); osc.setSawShape(20.0, 15.0);
    core.setSlideInPitchDomain(true); core.setSlideTimeConstant(120.0);
    const float P_TUNE = 0.5f, P_CUT = 0.35f, P_RES = 0.5f, P_ENV = 0.5f, P_DEC = 0.4f, P_ACC = 0.5f, P_WAVE = 1.0f;
    float accF = 0, accS = 0; bool gateHigh = false, slideHigh = false;
    auto effAcc = [&]() { double b = 100.0 * P_ACC; return std::min(100.0, b * (1.0 + 0.8 * (0.5 * accF + 0.5 * accS))); };
    auto update = [&]() {
        core.setTuning(400.0 + 80.0 * P_TUNE);
        core.setCutoff(314.0 * std::pow(2394.0 / 314.0, P_CUT));
        const double s3 = 1.0 - std::exp(-3.0); core.setResonance(100.0 * (-std::log(1.0 - P_RES * s3) / 3.0));
        core.setEnvMod(100.0 * P_ENV * P_ENV * P_ENV);
        const double lo = 200.0 / DT, hi = 2500.0 / DT; core.setDecay(lo * std::pow(hi / lo, P_DEC));
        core.setAccentDecay(lo); core.setAccent(effAcc()); core.setVolume(-6.0);
        core.setWaveform(P_WAVE); osc.setWaveform(P_WAVE); core.setPitchBend(0.0);
    };
    long total = (long)STEP_SAMPLES * 16 * BARS; std::vector<float> out(total);
    for (long i = 0; i < total; i++) {
        if (i % 16 == 0) update();
        int g, n, a, sl; cables(i, &g, &n, &a, &sl);
        bool gate = g, accent = a; slideHigh = sl;
        accF -= accF * (1.0f / sr) / 0.10f; accS -= accS * (1.0f / sr) / 0.45f;
        if (gate && !gateHigh) {
            if (accent) { core.setAccent(effAcc()); accF = std::min(1.f, accF + 0.55f); accS = std::min(1.f, accS + 0.55f); }
            core.noteOn(n, accent ? 127 : 64);
        } else if (!gate && gateHigh) { if (!slideHigh) core.allNotesOff(); }
        gateHigh = gate;
        float o = (float)core.getSample() * 5.f; o = 7.f * std::tanh(o / 7.f);
        out[i] = std::max(-12.f, std::min(12.f, o)) / 5.f;
    }
    wav(argc > 1 ? argv[1] : "ref.wav", out);
    return 0;
}
