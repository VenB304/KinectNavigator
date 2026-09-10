#pragma once
#include <math.h>

// Signal-conditioning primitive: a 1 Euro filter (Casiez et al., CHI 2012) that
// smooths the dominant hand's shoulder-relative position for the air d-pad.
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
