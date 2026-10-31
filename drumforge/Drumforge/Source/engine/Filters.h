#pragma once

#include <algorithm>
#include <cmath>

#include "Mapping.h"

namespace forja::drumforge::detail
{

inline constexpr double kPi = 3.14159265358979323846;

// Biquad RBJ em forma direta transposta II: coeficientes em double, estado por
// instancia. Nao aloca e nao chama libm no laco de amostras.
struct Biquad
{
    struct Coefficients
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
    };

    void reset() noexcept
    {
        z1 = 0.0;
        z2 = 0.0;
    }

    void setCoefficients (const Coefficients& newCoefficients) noexcept
    {
        coefficients = newCoefficients;
    }

    float process (float input) noexcept
    {
        const double x = (double) input;
        const double y = coefficients.b0 * x + z1;
        z1 = coefficients.b1 * x - coefficients.a1 * y + z2;
        z2 = coefficients.b2 * x - coefficients.a2 * y;
        return (float) y;
    }

private:
    Coefficients coefficients {};
    double z1 = 0.0;
    double z2 = 0.0;
};

namespace coefficients
{

inline Biquad::Coefficients normalised (double b0, double b1, double b2,
                                        double a0, double a1, double a2) noexcept
{
    Biquad::Coefficients result;

    if (! std::isfinite (a0) || std::abs (a0) < 1.0e-12)
        return result;

    result.b0 = b0 / a0;
    result.b1 = b1 / a0;
    result.b2 = b2 / a0;
    result.a1 = a1 / a0;
    result.a2 = a2 / a0;
    return result;
}

// Butterworth de 2a ordem (Q = 0,7071), 12 dB/oitava.
inline Biquad::Coefficients highPass (double sampleRate, double frequency) noexcept
{
    const double nyquistLimit = 0.45 * sampleRate;
    const double f0 = std::clamp (frequency, 5.0, nyquistLimit);
    const double w0 = 2.0 * kPi * f0 / sampleRate;
    const double cosw = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * 0.7071067811865476);

    return normalised ((1.0 + cosw) * 0.5, -(1.0 + cosw), (1.0 + cosw) * 0.5,
                       1.0 + alpha, -2.0 * cosw, 1.0 - alpha);
}

inline Biquad::Coefficients lowShelf (double sampleRate, double frequency, double gainDb) noexcept
{
    const double nyquistLimit = 0.45 * sampleRate;
    const double f0 = std::clamp (frequency, 10.0, nyquistLimit);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * f0 / sampleRate;
    const double cosw = std::cos (w0);
    const double sinw = std::sin (w0);
    const double sqrtA2alpha = 2.0 * std::sqrt (A) * (sinw * 0.5 * std::sqrt (2.0));

    return normalised (A * ((A + 1.0) - (A - 1.0) * cosw + sqrtA2alpha),
                       2.0 * A * ((A - 1.0) - (A + 1.0) * cosw),
                       A * ((A + 1.0) - (A - 1.0) * cosw - sqrtA2alpha),
                       (A + 1.0) + (A - 1.0) * cosw + sqrtA2alpha,
                       -2.0 * ((A - 1.0) + (A + 1.0) * cosw),
                       (A + 1.0) + (A - 1.0) * cosw - sqrtA2alpha);
}

inline Biquad::Coefficients highShelf (double sampleRate, double frequency, double gainDb) noexcept
{
    const double nyquistLimit = 0.45 * sampleRate;
    const double f0 = std::clamp (frequency, 10.0, nyquistLimit);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * f0 / sampleRate;
    const double cosw = std::cos (w0);
    const double sinw = std::sin (w0);
    const double sqrtA2alpha = 2.0 * std::sqrt (A) * (sinw * 0.5 * std::sqrt (2.0));

    return normalised (A * ((A + 1.0) + (A - 1.0) * cosw + sqrtA2alpha),
                       -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw),
                       A * ((A + 1.0) + (A - 1.0) * cosw - sqrtA2alpha),
                       (A + 1.0) - (A - 1.0) * cosw + sqrtA2alpha,
                       2.0 * ((A - 1.0) - (A + 1.0) * cosw),
                       (A + 1.0) - (A - 1.0) * cosw - sqrtA2alpha);
}

inline Biquad::Coefficients peaking (double sampleRate, double frequency, double gainDb,
                                     double q = 0.9) noexcept
{
    const double nyquistLimit = 0.45 * sampleRate;
    const double f0 = std::clamp (frequency, 10.0, nyquistLimit);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * f0 / sampleRate;
    const double cosw = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * std::max (0.05, q));

    return normalised (1.0 + alpha * A, -2.0 * cosw, 1.0 - alpha * A,
                       1.0 + alpha / A, -2.0 * cosw, 1.0 - alpha / A);
}

} // namespace coefficients

} // namespace forja::drumforge::detail
