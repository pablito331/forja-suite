#include "ForjaMeter.h"

#include "ForjaTheme.h"

namespace forja::ui
{

ForjaMeter::ForjaMeter()
{
    setInterceptsMouseClicks (true, false);
}

void ForjaMeter::setLevels (float peakLeft, float peakRight)
{
    updateChannel (left, peakLeft);
    updateChannel (right, peakRight);
    repaint();
}

void ForjaMeter::updateChannel (Channel& channel, float linearPeak)
{
    const float newDb = linearPeak > 0.0f ? linearToDb (linearPeak, -100.0f) : -100.0f;
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (newDb <= -99.9f)
        channel.db = juce::jmax (-100.0f, channel.db - 1.5f);
    else if (newDb > channel.db)
        channel.db = newDb;
    else
        channel.db = juce::jmax (newDb, channel.db - 1.2f);

    if (channel.db > channel.hold)
    {
        channel.hold = channel.db;
        channel.holdUntilMs = now + 900.0;
    }
    else if (now > channel.holdUntilMs)
    {
        channel.hold = juce::jmax (channel.db, channel.hold - 0.6f);
        if (channel.hold <= -60.0f)
            channel.hold = -100.0f;
    }
}

void ForjaMeter::setClip (bool isClipped)
{
    if (clipped != isClipped)
    {
        clipped = isClipped;
        repaint();
    }
}

juce::Rectangle<int> ForjaMeter::getClipBounds() const
{
    return { 0, getHeight() - 18, getWidth(), 18 };
}

float ForjaMeter::railNorm (float db) const
{
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

void ForjaMeter::paint (juce::Graphics& g)
{
    const float railTop = 24.0f;
    const float railBottom = (float) getHeight() - 38.0f;
    const float railHeight = railBottom - railTop;

    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.13f));
    g.setColour (colour::dim);
    g.drawText ("PEAK", 0, 0, getWidth() / 2, 9, juce::Justification::centredLeft, false);

    g.setFont (tracked (uiFont (7.0f, juce::Font::plain), 0.06f));
    g.setColour (juce::Colour (0xff59636f));
    g.drawText ("dBFS", getWidth() / 2, 0, getWidth() / 2, 9, juce::Justification::centredRight, false);

    constexpr int scaleWidth = 14;
    constexpr int railWidth = 10;
    constexpr int channelGap = 8;
    const int groupWidth = scaleWidth + 4 + railWidth + channelGap + railWidth;
    const int startX = (getWidth() - groupWidth) / 2;
    const int scaleX = startX;
    const int railLeftX = startX + scaleWidth + 4;
    const int railRightX = railLeftX + railWidth + channelGap;

    g.setFont (monoFont (6.5f));
    g.setColour (juce::Colour (0xff59636f));
    g.drawText ("0", scaleX, (int) railTop - 4, scaleWidth, 8, juce::Justification::centredRight, false);
    g.drawText ("-30", scaleX - 2, (int) (railTop + railHeight * 0.5f) - 4, scaleWidth + 2, 8,
                juce::Justification::centredRight, false);
    g.drawText ("-60", scaleX - 2, (int) railBottom - 4, scaleWidth + 2, 8,
                juce::Justification::centredRight, false);

    g.setFont (uiFont (7.0f, juce::Font::bold));
    g.setColour (colour::dim);
    g.drawText ("L", railLeftX - 4, (int) railTop - 12, railWidth + 8, 8, juce::Justification::centred, false);
    g.drawText ("R", railRightX - 4, (int) railTop - 12, railWidth + 8, 8, juce::Justification::centred, false);

    auto drawRail = [&] (Channel& channel, int x)
    {
        const juce::Rectangle<float> rail ((float) x, railTop, (float) railWidth, railHeight);

        g.setColour (colour::meterTrack);
        g.fillRoundedRectangle (rail, 2.0f);

        const float norm = railNorm (channel.db);
        if (norm > 0.0f)
        {
            const juce::Rectangle<float> fill (rail.getX(), rail.getBottom() - railHeight * norm,
                                               rail.getWidth(), railHeight * norm);
            juce::ColourGradient gradient (colour::meterSafe, rail.getX(), rail.getBottom(),
                                           colour::meterClip, rail.getX(), rail.getY(), false);
            gradient.addColour (0.55, colour::meterSafe);
            gradient.addColour (0.78, colour::meterCaution);
            gradient.addColour (0.90, colour::meterHot);
            g.setGradientFill (gradient);
            g.fillRect (fill);
        }

        g.setColour (colour::line);
        g.drawRoundedRectangle (rail.reduced (0.5f), 2.0f, 1.0f);

        const float holdNorm = railNorm (channel.hold);
        if (holdNorm > 0.0f)
        {
            const float y = rail.getBottom() - railHeight * holdNorm;
            g.setColour (colour::meterPeak.withAlpha (0.7f));
            g.fillRect (rail.getX() + 1.0f, juce::jmax (rail.getY(), y - 2.0f), rail.getWidth() - 2.0f, 2.0f);
        }
    };

    drawRail (left, railLeftX);
    drawRail (right, railRightX);

    g.setFont (monoFont (8.0f));
    g.setColour (colour::muted);
    const int readoutY = (int) railBottom + 5;
    g.drawText (formatDb (left.db), railLeftX - 22, readoutY, railWidth + 44, 10,
                juce::Justification::centred, false);
    g.drawText (formatDb (right.db), railRightX - 22, readoutY, railWidth + 44, 10,
                juce::Justification::centred, false);

    const auto clipBounds = getClipBounds().toFloat();
    g.setColour (clipped ? juce::Colour (0xff321d1b) : colour::meterTrack);
    g.fillRoundedRectangle (clipBounds, 2.0f);

    if (clipped)
    {
        g.setColour (colour::error.withAlpha (0.36f));
        g.fillRoundedRectangle (clipBounds.expanded (1.5f), 3.0f);
        g.setColour (juce::Colour (0xff321d1b));
        g.fillRoundedRectangle (clipBounds, 2.0f);
    }

    g.setColour (clipped ? juce::Colour (0xff71403b) : colour::lineStrong);
    g.drawRoundedRectangle (clipBounds.reduced (0.5f), 2.0f, 1.0f);

    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.14f));
    g.setColour (clipped ? colour::error : colour::dim);
    g.drawText ("CLIP", clipBounds, juce::Justification::centred, false);
}

void ForjaMeter::mouseUp (const juce::MouseEvent& event)
{
    if (getClipBounds().contains (event.getPosition()) && onClipReset != nullptr)
        onClipReset();
}

void ForjaMeter::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (getClipBounds().contains (event.getPosition())
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor);
}

} // namespace forja::ui
