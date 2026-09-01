#pragma once
#include <math.h>

// Phase 1 signal-conditioning primitives. Run in PARALLEL with the legacy
// dual-rate EMA + central-difference velocity so the two streams can be
// compared on replay -- see docs/notes/recognition-architecture-research.md.
// Nothing in the recogniser consumes these yet.
//
// Header-only (small, hot, one translation unit uses it).

// --- one first-order low-pass stage --------------------------------------
// Keeps its own previous filtered value AND previous raw value, matching the
// Casiez et al. reference 1e implementation (the derivative stage needs the
// previous RAW input; the value stage needs the previous FILTERED output).
class LowPass1
{
public:
    float Filter(float x, float alpha)
    {
        const float y = m_init ? (alpha * x + (1.0f - alpha) * m_y) : x;
        m_y = y; m_raw = x; m_init = true; m_hasRaw = true;
        return y;
    }
    bool  HasRaw() const { return m_hasRaw; }
    float Raw()    const { return m_raw; }
    void  Reset()  { m_init = m_hasRaw = false; m_y = m_raw = 0.0f; }
private:
    float m_y = 0.0f, m_raw = 0.0f;
    bool  m_init = false, m_hasRaw = false;
};

// --- 1 Euro filter (Casiez, Roussel & Vogel -- CHI 2012) ----------------
// Adaptive low-pass: heavy smoothing while the signal is slow (kills the
// resting / "inferred joint" jitter), cutoff opens up as it speeds so a real
// stroke passes with near-zero lag.
class OneEuro
{
public:
    void Configure(float minCutoff, float beta, float dCutoff)
    {
        m_minCutoff = minCutoff;
        m_beta      = beta;
        m_dCutoff   = dCutoff;
    }
    void Reset() { m_x.Reset(); m_dx.Reset(); }

    // dt in seconds. Returns the filtered value.
    float Filter(float x, float dt)
    {
        if (dt <= 0.0f) return x;
        const float dval  = m_x.HasRaw() ? (x - m_x.Raw()) / dt : 0.0f;
        const float edval = m_dx.Filter(dval, Alpha(m_dCutoff, dt));
        const float cut   = m_minCutoff + m_beta * fabsf(edval);
        return m_x.Filter(x, Alpha(cut, dt));
    }
private:
    static float Alpha(float cutoff, float dt)
    {
        const float tau = 1.0f / (2.0f * 3.14159265358979f * cutoff);
        return 1.0f / (1.0f + tau / dt);
    }
    float    m_minCutoff = 1.0f, m_beta = 0.05f, m_dCutoff = 1.0f;
    LowPass1 m_x, m_dx;
};

// --- 5-point Savitzky-Golay first derivative ---------------------------
// Fits a quadratic to the last 5 samples (least-squares) and differentiates
// it: a low-pass differentiator. Coefficients (-2,-1,0,1,2)/10. Centred, so
// the reported velocity trails the newest sample by 2 frames (~66 ms at
// 30 Hz) -- inside the 150-250 ms budget for discrete commands. A 1-2 frame
// "inferred" position spike can't produce a 9 m/s artifact here because the
// polynomial fit doesn't chase it.
class SavGol5
{
public:
    void Reset() { m_n = 0; for (float& v : m_buf) v = 0.0f; }

    // Push the newest sample; writes units/sec into `vel`. Returns false until
    // 5 samples have accumulated (vel = 0 until then).
    bool Push(float x, float dt, float& vel)
    {
        for (int i = 0; i < 4; ++i) m_buf[i] = m_buf[i + 1];
        m_buf[4] = x;
        if (m_n < 5) ++m_n;
        if (m_n < 5 || dt <= 0.0f) { vel = 0.0f; return false; }
        const float d = (-2.0f * m_buf[0] - m_buf[1] + m_buf[3] + 2.0f * m_buf[4]) / 10.0f;
        vel = d / dt;   // assumes ~uniform dt (true at 30 Hz; big gaps trigger a resync anyway)
        return true;
    }
private:
    float m_buf[5] = { 0, 0, 0, 0, 0 };
    int   m_n = 0;
};
