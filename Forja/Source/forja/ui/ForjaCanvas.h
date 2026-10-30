#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>

namespace forja::ui
{

// The editors are laid out once in design coordinates (1000x540 or 560x260).
// Resizing keeps that virtual canvas intact and scales every child, so layouts,
// knobs and hit areas stay proportional without a second layout pass.
struct CanvasMetrics
{
    float scale = 1.0f;
    juce::AffineTransform transform;
};

inline CanvasMetrics computeCanvasMetrics (int width, int height,
                                           int designWidth, int designHeight) noexcept
{
    CanvasMetrics metrics;

    if (width <= 0 || height <= 0 || designWidth <= 0 || designHeight <= 0)
        return metrics;

    metrics.scale = std::min ((float) width / (float) designWidth,
                              (float) height / (float) designHeight);

    const float offsetX = ((float) width - (float) designWidth * metrics.scale) * 0.5f;
    const float offsetY = ((float) height - (float) designHeight * metrics.scale) * 0.5f;

    metrics.transform = juce::AffineTransform::scale (metrics.scale).translated (offsetX, offsetY);
    return metrics;
}

inline void applyCanvasScale (juce::Component& editor, int designWidth, int designHeight)
{
    const auto metrics = computeCanvasMetrics (editor.getWidth(), editor.getHeight(),
                                               designWidth, designHeight);

    for (auto* child : editor.getChildren())
    {
        // The corner resizer lives in window pixels, not in the design canvas.
        if (dynamic_cast<juce::ResizableCornerComponent*> (child) != nullptr)
            continue;

        child->setTransform (metrics.transform);
    }
}

inline void configureResizableEditor (juce::AudioProcessorEditor& editor,
                                      juce::ComponentBoundsConstrainer& constrainer,
                                      int designWidth, int designHeight)
{
    constrainer.setFixedAspectRatio ((double) designWidth / (double) designHeight);
    constrainer.setSizeLimits (juce::roundToInt ((float) designWidth * 0.7f),
                               juce::roundToInt ((float) designHeight * 0.7f),
                               designWidth * 2, designHeight * 2);

    editor.setConstrainer (&constrainer);
    editor.setResizable (true, true);
}

} // namespace forja::ui
