#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace forja::ui
{

class ForjaEqCurve : public juce::Component
{
public:
    struct Point
    {
        float frequency = 0.0f;
        float db = 0.0f;
    };

    struct Marker
    {
        float frequency = 0.0f;
        float dbOffset = 0.0f;
        juce::String label;
        juce::String id;
        bool draggable = false;
        bool frequencyOnly = false;
    };

    ForjaEqCurve();

    void setCurve (std::vector<Point> points);
    void setMarkers (std::vector<Marker> markers);
    void setCaption (juce::String text);
    void setBypassed (bool shouldBeBypassed);

    // Arraste de nós: o host do plugin recebe a frequência absoluta (x) e o
    // deslocamento em dB desde o início do arraste (y), para mapear sem salto.
    std::function<void (const juce::String& id)> onMarkerDragStart;
    std::function<void (const juce::String& id, float frequency, float dbDelta)> onMarkerDrag;
    std::function<void()> onMarkerDragEnd;

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

private:
    juce::Rectangle<float> getPlotBounds() const;
    juce::Point<float> toScreen (float frequency, float db,
                                 juce::Rectangle<float> plot) const;
    float dbAt (float frequency) const;
    float frequencyForX (float x, juce::Rectangle<float> plot) const;
    float dbForY (float y, juce::Rectangle<float> plot) const;
    int findMarkerAt (juce::Point<float> position, juce::Rectangle<float> plot) const;

    std::vector<Point> curve;
    std::vector<Marker> markers;
    juce::String caption;
    bool bypassed = false;

    int dragIndex = -1;
    int hoverIndex = -1;
    float dragStartDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForjaEqCurve)
};

} // namespace forja::ui
