#pragma once

#include <juce_graphics/juce_graphics.h>

#include <cmath>
#include <utility>
#include <vector>

namespace forja::ui
{

namespace colour
{
inline const juce::Colour iron0 { 0xff0d0f13 };
inline const juce::Colour iron1 { 0xff13161c };
inline const juce::Colour iron2 { 0xff191d24 };
inline const juce::Colour iron3 { 0xff20252d };
inline const juce::Colour iron4 { 0xff282e37 };
inline const juce::Colour line { 0xff303743 };
inline const juce::Colour lineStrong { 0xff3b4451 };
inline const juce::Colour text { 0xffeceef1 };
inline const juce::Colour muted { 0xff9aa3ae };
inline const juce::Colour dim { 0xff68727f };
inline const juce::Colour ember { 0xffff7a18 };
inline const juce::Colour emberSoft { 0xffffb05a };
inline const juce::Colour emberPale { 0xffffdcae };
inline const juce::Colour success { 0xff76b58b };
inline const juce::Colour error { 0xffdb6d60 };
inline const juce::Colour meterTrack { 0xff101318 };
inline const juce::Colour meterSafe { 0xff76b58b };
inline const juce::Colour meterCaution { 0xffffb05a };
inline const juce::Colour meterHot { 0xffff7a18 };
inline const juce::Colour meterClip { 0xffdb6d60 };
inline const juce::Colour meterPeak { 0xffeceef1 };
inline const juce::Colour graphGrid { 0xff2a313b };
inline const juce::Colour graphAxis { 0xff3b4451 };
} // namespace colour

inline juce::FontOptions withPlatformFallbacks (const char* family, float height, int style,
                                               std::vector<juce::String> fallbacks)
{
    return juce::FontOptions (family, height, style)
        .withFallbacks (std::move (fallbacks))
        .withFallbackEnabled (true);
}

inline juce::Font uiFont (float height, int style = juce::Font::plain)
{
    return juce::Font (withPlatformFallbacks ("Segoe UI", height, style,
                                             { "Arial", "Helvetica Neue", "DejaVu Sans",
                                               "Liberation Sans", "Noto Sans" }));
}

inline juce::Font displayFont (float height)
{
    return juce::Font (withPlatformFallbacks ("Georgia", height, juce::Font::plain,
                                             { "Times New Roman", "DejaVu Serif",
                                               "Liberation Serif", "Noto Serif" }));
}

inline juce::Font monoFont (float height)
{
    return juce::Font (withPlatformFallbacks ("Consolas", height, juce::Font::plain,
                                             { "Menlo", "DejaVu Sans Mono", "Liberation Mono",
                                               "Noto Sans Mono" }));
}

inline juce::Font tracked (juce::Font font, float em)
{
    return font.withExtraKerningFactor (em);
}

/* juce::String(const char*) assume Latin-1; literais UTF-8 precisam passar por aqui. */
inline juce::String utf8 (const char* text)
{
    return juce::String::fromUTF8 (text);
}

/* Link agregador das redes da Forja. Enquanto estiver vazio, o chip REDES fica oculto. */
inline constexpr const char* kSocialUrl = "https://bnd.link/pablito31";

inline juce::String infinityText()
{
    return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92\xe2\x88\x9e"));
}

inline juce::String formatDb (float db)
{
    if (! std::isfinite (db) || db <= -60.0f)
        return infinityText();

    return juce::String (db > 0.0f ? "+" : "") + juce::String (db, 1);
}

inline float linearToDb (float linear, float floorDb = -100.0f)
{
    if (linear <= 0.0f)
        return floorDb;

    return std::max (floorDb, 20.0f * std::log10 (linear));
}

} // namespace forja::ui
