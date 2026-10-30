#pragma once

// Minimal VST2 surface used only by the Airwindows reference code in Tests.
// It intentionally does not implement hosting, chunks or parameter automation
// beyond what the reference processing code touches.

#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef __audioeffect__
#define __audioeffect__
#endif

typedef int VstInt32;
typedef void* audioMasterCallback;

enum VstPlugCategory
{
    kPlugCategEffect = 0
};

constexpr int kVstMaxProgNameLen = 24;
constexpr int kVstMaxParamStrLen = 32;
constexpr int kVstMaxProductStrLen = 64;
constexpr int kVstMaxVendorStrLen = 64;

inline void vst_strncpy (char* dest, const char* src, int maxLen) noexcept
{
    if (dest == nullptr || src == nullptr || maxLen <= 0)
        return;

    std::strncpy (dest, src, (std::size_t) maxLen - 1);
    dest[maxLen - 1] = '\0';
}

inline void int2string (VstInt32 value, char* text, int maxLen) noexcept
{
    if (text == nullptr || maxLen <= 0)
        return;

    std::snprintf (text, (std::size_t) maxLen, "%d", (int) value);
}

inline void float2string (float value, char* text, int maxLen) noexcept
{
    if (text == nullptr || maxLen <= 0)
        return;

    std::snprintf (text, (std::size_t) maxLen, "%.6f", (double) value);
}

class AudioEffectX
{
public:
    AudioEffectX (audioMasterCallback, VstInt32, VstInt32) {}
    virtual ~AudioEffectX() = default;

    virtual void processReplacing (float**, float**, VstInt32) {}
    virtual void processDoubleReplacing (double**, double**, VstInt32) {}
    virtual void setParameter (VstInt32, float) {}
    virtual float getParameter (VstInt32) { return 0.0f; }

    double getSampleRate() const noexcept { return sampleRate; }
    void setSampleRate (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    void setNumInputs (VstInt32) {}
    void setNumOutputs (VstInt32) {}
    void setUniqueID (unsigned long) {}
    void canProcessReplacing (bool = true) {}
    void canDoubleReplacing (bool = true) {}
    void programsAreChunks (bool) {}

private:
    double sampleRate = 48000.0;
};

using AudioEffect = AudioEffectX;
