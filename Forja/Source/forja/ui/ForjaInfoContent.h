#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace forja::ui
{

struct ForjaInfoControl
{
    juce::String name;
    juce::String range;
    juce::String description;
};

struct ForjaInfoSection
{
    juce::String heading;
    std::vector<juce::String> paragraphs;
};

struct ForjaInfoProvenance
{
    juce::String source;
    juce::String author;
    juce::String license;
    juce::String commit;
    juce::String notes;
};

struct ForjaInfo
{
    juce::String title;
    juce::String subtitle;
    juce::String tagline;
    std::vector<ForjaInfoSection> sections;
    std::vector<ForjaInfoControl> controls;
    ForjaInfoProvenance provenance;
};

} // namespace forja::ui
