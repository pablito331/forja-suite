#include "ForjaGainReductionMeter.h"

#include "ForjaTheme.h"

namespace forja::ui
{

void ForjaGainReductionMeter::setGainReductionDb (float leftDb, float rightDb)
{
    updateChannel (left, leftDb);
    updateChannel (right, rightDb);
    repaint();
}

void ForjaGainReductionMeter::setBypassed (bool shouldBeBypassed)
{
    if (bypassed == shouldBeBypassed)
        return;

    bypassed = shouldBeBypassed;
    repaint();
}

void ForjaGainReductionMeter::updateChannel (Channel& channel, float db)
{
    const float target = juce::jlimit (-24.0f, 0.0f, db);
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (target < channel.db)
        channel.db = target;
    else
        channel.db = juce::jmin (target, channel.db + 1.2f);

    if (channel.db < channel.hold)
    {
        channel.hold = channel.db;
        channel.holdUntilMs = now + 900.0;
    }
    else if (now > channel.holdUntilMs)
    {
        channel.hold = juce::jmin (channel.db, channel.hold + 0.6f);
        if (channel.hold > 0.0f)
            channel.hold = 0.0f;
    }
}

float ForjaGainReductionMeter::railNorm (float db) const
{
    return juce::jlimit (0.0f, 1.0f, -db / 24.0f);
}

void ForjaGainReductionMeter::paint (juce::Graphics& g)
{
    const float railTop = 24.0f;
    const float railBottom = (float) getHeight() - 36.0f;
    const float railHeight = juce::jmax (1.0f, railBottom - railTop);

    g.setFont (tracked (uiFont (7.0f, juce::Font::bold), 0.13f));
    g.setColour (bypassed ? colour::dim.withAlpha (0.6f) : colour::dim);
    g.drawText ("GR", 0, 0, getWidth() / 2, 9, juce::Justification::centredLeft, false);

    g.setFont (tracked (uiFont (7.0f, juce::Font::plain), 0.06f));
    g.setColour (juce::Colour (0xff59636f));
    g.drawText ("dB", getWidth() / 2, 0, getWidth() / 2, 9, juce::Justification::centredRight, false);

    constexpr int scaleWidth = 16;
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
    g.drawText ("-12", scaleX - 2, (int) (railTop + railHeight * 0.5f) - 4, scaleWidth + 2, 8,
                juce::Justification::centredRight, false);
    g.drawText ("-24", scaleX - 2, (int) railBottom - 4, scaleWidth + 2, 8,
                juce::Justification::centredRight, false);

    g.setFont (uiFont (7.0f, juce::Font::bold));
    g.setColour (colour::dim);
    g.drawText ("L", railLeftX - 4, (int) railTop - 12, railWidth + 8, 8, juce::Justification::centred, false);
    g.drawText ("R", railRightX - 4, (int) railTop - 12, railWidth + 8, 8, juce::Justification::centred, false);

    auto drawRail = [&] (const Channel& channel, int x)
    {
        const juce::Rectangle<float> rail ((float) x, railTop, (float) railWidth, railHeight);

        g.setColour (colour::meterTrack);
        g.fillRoundedRectangle (rail, 2.0f);

        const float norm = railNorm (channel.db);
        if (norm > 0.0f)
        {
            const juce::Rectangle<float> fill (rail.getX(), rail.getY(),
                                               rail.getWidth(), railHeight * norm);
            juce::ColourGradient gradient (colour::emberSoft, rail.getX(), rail.getY(),
                                           colour::ember, rail.getX(), rail.getBottom(), false);
            g.setGradientFill (gradient);
            g.fillRect (fill);
        }

        g.setColour (colour::line);
        g.drawRoundedRectangle (rail.reduced (0.5f), 2.0f, 1.0f);

        const float holdNorm = railNorm (channel.hold);
        if (holdNorm > 0.0f)
        {
            const float y = rail.getY() + railHeight * holdNorm;
            g.setColour (colour::meterPeak.withAlpha (0.7f));
            g.fillRect (rail.getX() + 1.0f, juce::jlimit (rail.getY(), rail.getBottom() - 2.0f, y - 2.0f),
                        rail.getWidth() - 2.0f, 2.0f);
        }
    };

    drawRail (left, railLeftX);
    drawRail (right, railRightX);

    g.setFont (monoFont (8.0f));
    g.setColour (bypassed ? colour::dim : colour::muted);
    const int readoutY = (int) railBottom + 5;
    auto readout = [] (float db)
    {
        if (db > -0.05f)
            return juce::String ("0.0");
        return juce::String (db, 1);
    };
    g.drawText (readout (left.db), railLeftX - 20, readoutY, railWidth + 40, 10,
                juce::Justification::centred, false);
    g.drawText (readout (right.db), railRightX - 20, readoutY, railWidth + 40, 10,
                juce::Justification::centred, false);
}

} // namespace forja::ui
