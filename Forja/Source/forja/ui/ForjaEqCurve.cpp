#include "ForjaEqCurve.h"

#include "ForjaSpectrum.h"
#include "ForjaTheme.h"

#include <algorithm>
#include <cmath>

namespace forja::ui
{

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float maxDb = 18.0f;
constexpr float logSpan = 3.0f; // log10 (20000 / 20)

struct GridLine
{
    float frequency;
    const char* label;
};

const GridLine verticalLines[] =
{
    { 30.0f, "30" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" },
    { 300.0f, nullptr }, { 500.0f, "500" }, { 1000.0f, "1k" }, { 2000.0f, "2k" },
    { 3000.0f, nullptr }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
};

const float horizontalDb[] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };

juce::String formatFrequency (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, hz < 10000.0f ? 1 : 0) + "k";

    return juce::String (juce::roundToInt (hz));
}
} // namespace

ForjaEqCurve::ForjaEqCurve()
{
    setInterceptsMouseClicks (true, false);
}

void ForjaEqCurve::setCurve (std::vector<Point> points)
{
    curve = std::move (points);
    repaint();
}

void ForjaEqCurve::setSpectrum (const float* bandDb, int count)
{
    spectrum.resize (static_cast<std::size_t> (std::max (0, count)));

    for (int i = 0; i < count; ++i)
        spectrum[static_cast<std::size_t> (i)] = bandDb[i];

    repaint();
}

void ForjaEqCurve::setMarkers (std::vector<Marker> newMarkers)
{
    markers = std::move (newMarkers);
    repaint();
}

void ForjaEqCurve::setCaption (juce::String text)
{
    caption = std::move (text);
    repaint();
}

void ForjaEqCurve::setBypassed (bool shouldBeBypassed)
{
    if (bypassed == shouldBeBypassed)
        return;

    bypassed = shouldBeBypassed;
    repaint();
}

juce::Rectangle<float> ForjaEqCurve::getPlotBounds() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (24.0f)
                                           .withTrimmedRight (6.0f)
                                           .withTrimmedTop (6.0f)
                                           .withTrimmedBottom (14.0f);
}

juce::Point<float> ForjaEqCurve::toScreen (float frequency, float db,
                                           juce::Rectangle<float> plot) const
{
    const float clamped = std::clamp (frequency, minFrequency, maxFrequency);
    const float normX = std::log10 (clamped / minFrequency) / logSpan;
    const float normY = std::clamp (db / maxDb, -1.0f, 1.0f);

    return { plot.getX() + normX * plot.getWidth(),
             plot.getCentreY() - normY * plot.getHeight() * 0.5f };
}

float ForjaEqCurve::frequencyForX (float x, juce::Rectangle<float> plot) const
{
    const float norm = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return minFrequency * std::pow (1000.0f, norm);
}

float ForjaEqCurve::dbForY (float y, juce::Rectangle<float> plot) const
{
    const float norm = (plot.getCentreY() - y) / (plot.getHeight() * 0.5f);
    return juce::jlimit (-maxDb, maxDb, norm * maxDb);
}

int ForjaEqCurve::findMarkerAt (juce::Point<float> position, juce::Rectangle<float> plot) const
{
    for (int i = (int) markers.size() - 1; i >= 0; --i)
    {
        const auto& marker = markers[(std::size_t) i];

        if (! marker.draggable)
            continue;

        const auto screen = toScreen (marker.frequency,
                                      dbAt (marker.frequency) + marker.dbOffset, plot);

        if (position.getDistanceFrom (screen) <= 13.0f)
            return i;
    }

    return -1;
}

float ForjaEqCurve::dbAt (float frequency) const
{
    if (curve.empty())
        return 0.0f;

    if (frequency <= curve.front().frequency)
        return curve.front().db;
    if (frequency >= curve.back().frequency)
        return curve.back().db;

    for (std::size_t i = 1; i < curve.size(); ++i)
    {
        if (curve[i].frequency >= frequency)
        {
            const auto& a = curve[i - 1];
            const auto& b = curve[i];
            const float span = b.frequency - a.frequency;
            const float t = span > 0.0f ? (frequency - a.frequency) / span : 0.0f;
            return a.db + (b.db - a.db) * t;
        }
    }

    return curve.back().db;
}

void ForjaEqCurve::paint (juce::Graphics& g)
{
    const auto plot = getPlotBounds();

    g.setColour (colour::meterTrack);
    g.fillRoundedRectangle (plot.expanded (0.5f), 3.0f);

    for (const auto& line : verticalLines)
    {
        const auto point = toScreen (line.frequency, 0.0f, plot);
        g.setColour (line.frequency == 1000.0f ? colour::graphAxis : colour::graphGrid);
        g.drawVerticalLine (juce::roundToInt (point.x), plot.getY(), plot.getBottom());

        if (line.label != nullptr)
        {
            g.setFont (monoFont (7.0f));
            g.setColour (colour::dim);
            g.drawText (line.label, juce::roundToInt (point.x) - 10, (int) plot.getBottom() + 1,
                        20, 11, juce::Justification::centred, false);
        }
    }

    for (float db : horizontalDb)
    {
        const auto point = toScreen (1000.0f, db, plot);
        g.setColour (db == 0.0f ? colour::graphAxis : colour::graphGrid);
        g.drawHorizontalLine (juce::roundToInt (point.y), plot.getX(), plot.getRight());

        g.setFont (monoFont (7.0f));
        g.setColour (db == 0.0f ? colour::muted : colour::dim);
        g.drawText (db > 0.0f ? "+" + juce::String ((int) db) : juce::String ((int) db),
                    0, juce::roundToInt (point.y) - 6, 21, 12,
                    juce::Justification::centredRight, false);
    }

    if (! spectrum.empty())
    {
        // RTA atras da curva: e mediacao da entrada, escala propria do piso
        // do analisador ate 0 dBFS, desenhado como area discreta para nao
        // competir com a curva em ember.
        const int bands = static_cast<int> (spectrum.size());
        const float floor = ForjaSpectrum::floorDb;

        juce::Path area;
        const auto bandX = [bands, &plot] (int i)
        {
            const float norm = (static_cast<float> (i) + 0.5f) / static_cast<float> (bands);
            return plot.getX() + norm * plot.getWidth();
        };

        area.startNewSubPath (bandX (0), plot.getBottom());

        for (int i = 0; i < bands; ++i)
        {
            const float norm = std::clamp ((spectrum[static_cast<std::size_t> (i)] - floor)
                                           / (0.0f - floor), 0.0f, 1.0f);
            area.lineTo (bandX (i), plot.getBottom() - norm * plot.getHeight());
        }

        area.lineTo (bandX (bands - 1), plot.getBottom());
        area.closeSubPath();

        g.setGradientFill (juce::ColourGradient (colour::muted.withAlpha (0.22f),
                                                 plot.getCentreX(), plot.getY(),
                                                 colour::muted.withAlpha (0.05f),
                                                 plot.getCentreX(), plot.getBottom(), false));
        g.fillPath (area);
    }

    if (caption.isNotEmpty())
    {
        g.setFont (tracked (uiFont (6.5f, juce::Font::bold), 0.12f));
        g.setColour (colour::dim);
        g.drawText (caption.toUpperCase(), (int) plot.getX(), 4, (int) plot.getWidth() - 4, 10,
                    juce::Justification::centredRight, false);
    }

    if (curve.empty())
    {
        g.setFont (uiFont (9.0f));
        g.setColour (colour::dim);
        g.drawText ("ajuste um controle para medir a curva",
                    plot.toNearestInt(), juce::Justification::centred, false);
        return;
    }

    juce::Path line;
    juce::Path fill;
    bool started = false;

    for (const auto& point : curve)
    {
        const auto screen = toScreen (point.frequency, point.db, plot);

        if (! started)
        {
            line.startNewSubPath (screen);
            fill.startNewSubPath (screen.x, plot.getCentreY());
            fill.lineTo (screen);
            started = true;
        }
        else
        {
            line.lineTo (screen);
            fill.lineTo (screen);
        }
    }

    fill.lineTo (toScreen (maxFrequency, 0.0f, plot).x, plot.getCentreY());
    fill.closeSubPath();

    const auto curveColour = bypassed ? colour::dim : colour::ember;

    g.setGradientFill (juce::ColourGradient (curveColour.withAlpha (0.26f), plot.getCentreX(),
                                             plot.getCentreY(), curveColour.withAlpha (0.02f),
                                             plot.getCentreX(), plot.getBottom(), false));
    g.fillPath (fill);

    g.setColour (curveColour);
    g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    for (int i = 0; i < (int) markers.size(); ++i)
    {
        const auto& marker = markers[(std::size_t) i];
        const auto screen = toScreen (marker.frequency, dbAt (marker.frequency) + marker.dbOffset, plot);
        const bool active = marker.draggable && (i == dragIndex || i == hoverIndex);

        g.setColour (colour::emberPale.withAlpha (active ? 0.34f : 0.22f));
        g.fillEllipse (screen.x - (active ? 7.0f : 5.0f), screen.y - (active ? 7.0f : 5.0f),
                       active ? 14.0f : 10.0f, active ? 14.0f : 10.0f);
        g.setColour (active ? colour::emberSoft : colour::emberPale);
        g.fillEllipse (screen.x - 2.2f, screen.y - 2.2f, 4.4f, 4.4f);

        if (marker.label.isNotEmpty())
        {
            const auto text = marker.label + " " + formatFrequency (marker.frequency);
            g.setFont (monoFont (7.0f));
            g.setColour (active ? colour::emberPale : colour::dim);

            const int textWidth = juce::roundToInt (
                juce::GlyphArrangement::getStringWidth (monoFont (7.0f), text));
            const bool placeLeft = screen.x + 8.0f + textWidth > plot.getRight();

            g.drawText (text,
                        placeLeft ? juce::roundToInt (screen.x) - textWidth - 8
                                  : juce::roundToInt (screen.x) + 8,
                        juce::roundToInt (screen.y) - 14, textWidth, 11,
                        juce::Justification::centredLeft, false);
        }
    }
}

void ForjaEqCurve::mouseMove (const juce::MouseEvent& event)
{
    const auto plot = getPlotBounds();
    const int index = findMarkerAt (event.position, plot);

    if (index != hoverIndex)
    {
        hoverIndex = index;
        setMouseCursor (index >= 0 ? juce::MouseCursor::DraggingHandCursor
                                   : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ForjaEqCurve::mouseDown (const juce::MouseEvent& event)
{
    const auto plot = getPlotBounds();
    dragIndex = findMarkerAt (event.position, plot);

    if (dragIndex < 0)
        return;

    dragStartDb = dbForY (event.position.y, plot);

    if (onMarkerDragStart != nullptr)
        onMarkerDragStart (markers[(std::size_t) dragIndex].id);

    repaint();
}

void ForjaEqCurve::mouseDrag (const juce::MouseEvent& event)
{
    if (dragIndex < 0 || dragIndex >= (int) markers.size())
        return;

    const auto plot = getPlotBounds();
    auto& marker = markers[(std::size_t) dragIndex];
    const float frequency = frequencyForX (event.position.x, plot);
    const float dbDelta = marker.frequencyOnly
                            ? 0.0f
                            : dbForY (event.position.y, plot) - dragStartDb;

    marker.frequency = frequency;
    marker.dbOffset = dbDelta;

    if (onMarkerDrag != nullptr)
        onMarkerDrag (marker.id, frequency, dbDelta);

    repaint();
}

void ForjaEqCurve::mouseUp (const juce::MouseEvent&)
{
    if (dragIndex < 0)
        return;

    if (dragIndex < (int) markers.size())
        markers[(std::size_t) dragIndex].dbOffset = 0.0f;

    dragIndex = -1;

    if (onMarkerDragEnd != nullptr)
        onMarkerDragEnd();

    repaint();
}

void ForjaEqCurve::mouseExit (const juce::MouseEvent&)
{
    if (hoverIndex != -1)
    {
        hoverIndex = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

} // namespace forja::ui
