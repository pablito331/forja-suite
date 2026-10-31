#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>

namespace
{

juce::String dbText (float db, int decimals = 1)
{
    return (db > 0.0f ? "+" : "") + juce::String (db, decimals) + " dB";
}

juce::String panText (float value)
{
    const int percent = juce::roundToInt ((value - 0.5f) * 200.0f);

    if (percent == 0)
        return "C";

    return (percent < 0 ? "L" : "R") + juce::String (std::abs (percent));
}

std::unique_ptr<juce::AudioParameterFloat> makeUnitParameter (
    const juce::String& id,
    const juce::String& name,
    float defaultValue,
    std::function<juce::String (float)> formatter)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel (id)
            .withStringFromValueFunction ([formatter = std::move (formatter)] (float value, int)
            {
                if (formatter != nullptr)
                    return formatter (value);

                return juce::String (value, 2);
            }));
}

std::unique_ptr<juce::AudioParameterBool> makeBoolParameter (const juce::String& id,
                                                             const juce::String& name,
                                                             bool defaultValue = false)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name,
                                                       defaultValue);
}

} // namespace

using forja::drumforge::kNumPads;
using forja::drumforge::mapping::eqDb;
using forja::drumforge::mapping::faderDb;
using forja::drumforge::mapping::hpfHz;
using forja::drumforge::mapping::ratio;
using forja::drumforge::mapping::thresholdDb;
using forja::drumforge::mapping::trimDb;

DrumforgeAudioProcessor::DrumforgeAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Aux 1", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Aux 2", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Aux 3", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Aux 4", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      fallbackKit (forja::drumforge::Kit::createDefault())
{
    aliveFlag = std::make_shared<std::atomic<bool>> (true);
    cacheParameterPointers();

    startTimerHz (4);
}

DrumforgeAudioProcessor::~DrumforgeAudioProcessor()
{
    stopTimer();

    if (aliveFlag != nullptr)
        aliveFlag->store (false, std::memory_order_release);

    store.cancel();
}

juce::String DrumforgeAudioProcessor::padId (int padIndex, const juce::String& suffix)
{
    return "pad" + juce::String (padIndex + 1).paddedLeft ('0', 2) + "_" + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumforgeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto label = "Pad " + juce::String (pad + 1) + " ";
        const auto trim = [pad] (const char* suffix) { return padId (pad, suffix); };

        layout.add (makeUnitParameter (trim ("trim"), label + "Trim", 0.5f,
                                       [] (float value) { return dbText (trimDb (value)); }));
        layout.add (makeUnitParameter (trim ("fader"), label + "Fader", 0.91f,
                                       [] (float value) { return dbText (faderDb (value)); }));
        layout.add (makeUnitParameter (trim ("pan"), label + "Pan", 0.5f,
                                       [] (float value) { return panText (value); }));
        layout.add (makeUnitParameter (trim ("hpf"), label + "HPF", 0.0f,
                                       [] (float value)
                                       {
                                           const auto hz = hpfHz (value);
                                           return hz < 25.0f ? juce::String ("OFF")
                                                             : juce::String (juce::roundToInt (hz)) + " Hz";
                                       }));
        layout.add (makeUnitParameter (trim ("eqLow"), label + "EQ Low", 0.5f,
                                       [] (float value) { return dbText (eqDb (value)); }));
        layout.add (makeUnitParameter (trim ("eqMid"), label + "EQ Mid", 0.5f,
                                       [] (float value) { return dbText (eqDb (value)); }));
        layout.add (makeUnitParameter (trim ("eqHigh"), label + "EQ High", 0.5f,
                                       [] (float value) { return dbText (eqDb (value)); }));
        layout.add (makeUnitParameter (trim ("compThr"), label + "Comp Threshold", 0.7f,
                                       [] (float value) { return dbText (thresholdDb (value)); }));
        layout.add (makeUnitParameter (trim ("compRatio"), label + "Comp Ratio", 0.1f,
                                       [] (float value)
                                       {
                                           return juce::String (ratio (value), 1) + " : 1";
                                       }));
        layout.add (makeUnitParameter (trim ("send1"), label + "Send FX1", 0.0f, nullptr));
        layout.add (makeUnitParameter (trim ("send2"), label + "Send FX2", 0.0f, nullptr));

        layout.add (makeBoolParameter (trim ("mute"), label + "Mute"));
        layout.add (makeBoolParameter (trim ("solo"), label + "Solo"));
        layout.add (makeBoolParameter (trim ("phase"), label + "Phase Reverse"));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { trim ("out"), 1 }, label + "Output",
            juce::StringArray { "Master", "Aux 1", "Aux 2", "Aux 3", "Aux 4" }, 0));
    }

    layout.add (makeUnitParameter ("master_trim", "Master Trim", 0.5f,
                                   [] (float value) { return dbText (trimDb (value)); }));
    layout.add (makeUnitParameter ("master_comp", "Master Comp", 0.0f, nullptr));
    layout.add (makeBoolParameter ("master_limiter", "Master Limiter", true));
    layout.add (makeUnitParameter ("master_fx1Regen", "FX1 Regen", 0.5f, nullptr));
    layout.add (makeUnitParameter ("master_fx1Position", "FX1 Position", 1.0f, nullptr));
    layout.add (makeUnitParameter ("master_fx2Drive", "FX2 Drive", 0.2f, nullptr));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // Preview de loops MIDI: sync com o host por padrao, BPM local em 120.
    layout.add (makeBoolParameter ("loop_sync", "Loop Sync", true));
    layout.add (makeUnitParameter ("loop_bpm", "Loop BPM", 0.4f,
                                   [] (float value)
                                   {
                                       return juce::String (juce::roundToInt (40.0f + 200.0f * value))
                                              + " BPM";
                                   }));

    return layout;
}

void DrumforgeAudioProcessor::cacheParameterPointers()
{
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        auto& pointers = padPointers[(size_t) pad];

        pointers.trim = apvts.getRawParameterValue (padId (pad, "trim"));
        pointers.fader = apvts.getRawParameterValue (padId (pad, "fader"));
        pointers.pan = apvts.getRawParameterValue (padId (pad, "pan"));
        pointers.hpf = apvts.getRawParameterValue (padId (pad, "hpf"));
        pointers.eqLow = apvts.getRawParameterValue (padId (pad, "eqLow"));
        pointers.eqMid = apvts.getRawParameterValue (padId (pad, "eqMid"));
        pointers.eqHigh = apvts.getRawParameterValue (padId (pad, "eqHigh"));
        pointers.compThreshold = apvts.getRawParameterValue (padId (pad, "compThr"));
        pointers.compRatio = apvts.getRawParameterValue (padId (pad, "compRatio"));
        pointers.send1 = apvts.getRawParameterValue (padId (pad, "send1"));
        pointers.send2 = apvts.getRawParameterValue (padId (pad, "send2"));
        pointers.mute = apvts.getRawParameterValue (padId (pad, "mute"));
        pointers.solo = apvts.getRawParameterValue (padId (pad, "solo"));
        pointers.phase = apvts.getRawParameterValue (padId (pad, "phase"));
        pointers.output = apvts.getRawParameterValue (padId (pad, "out"));
    }

    masterPointers.trim = apvts.getRawParameterValue ("master_trim");
    masterPointers.comp = apvts.getRawParameterValue ("master_comp");
    masterPointers.limiter = apvts.getRawParameterValue ("master_limiter");
    masterPointers.fx1Regen = apvts.getRawParameterValue ("master_fx1Regen");
    masterPointers.fx1Position = apvts.getRawParameterValue ("master_fx1Position");
    masterPointers.fx2Drive = apvts.getRawParameterValue ("master_fx2Drive");
}

forja::drumforge::EngineParameters DrumforgeAudioProcessor::readEngineParameters() const
{
    forja::drumforge::EngineParameters parameters;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto& pointers = padPointers[(size_t) pad];
        auto& target = parameters.pads[(size_t) pad];

        target.trim = pointers.trim->load();
        target.fader = pointers.fader->load();
        target.pan = pointers.pan->load();
        target.hpf = pointers.hpf->load();
        target.eqLow = pointers.eqLow->load();
        target.eqMid = pointers.eqMid->load();
        target.eqHigh = pointers.eqHigh->load();
        target.compThreshold = pointers.compThreshold->load();
        target.compRatio = pointers.compRatio->load();
        target.send1 = pointers.send1->load();
        target.send2 = pointers.send2->load();
        target.mute = pointers.mute->load() > 0.5f;
        target.solo = pointers.solo->load() > 0.5f;
        target.phase = pointers.phase->load() > 0.5f;
        target.output = juce::jlimit (0, forja::drumforge::kNumOutputBuses - 1,
                                      (int) pointers.output->load());
    }

    parameters.master.trim = masterPointers.trim->load();
    parameters.master.comp = masterPointers.comp->load();
    parameters.master.limiter = masterPointers.limiter->load() > 0.5f;
    parameters.master.fx1Regen = masterPointers.fx1Regen->load();
    parameters.master.fx1Position = masterPointers.fx1Position->load();
    parameters.master.fx2Drive = masterPointers.fx2Drive->load();
    return parameters;
}

void DrumforgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    loopPlayer.prepare (sampleRate);
    setLatencySamples (0);
}

void DrumforgeAudioProcessor::releaseResources()
{
}

bool DrumforgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
    {
        const auto& set = layouts.outputBuses[bus];

        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void DrumforgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    std::array<juce::AudioBuffer<float>, forja::drumforge::kNumOutputBuses> busBuffers;
    std::array<juce::AudioBuffer<float>*, forja::drumforge::kNumOutputBuses> outputs {};
    const int busCount = juce::jmin (getBusCount (false), forja::drumforge::kNumOutputBuses);

    for (int bus = 0; bus < busCount; ++bus)
    {
        auto* audioBus = getBus (false, bus);

        if (audioBus != nullptr && audioBus->isEnabled())
        {
            // getBusBuffer devolve uma view do buffer principal (sem copiar audio).
            busBuffers[(size_t) bus] = getBusBuffer (buffer, false, bus);
            outputs[(size_t) bus] = &busBuffers[(size_t) bus];
        }
    }

    engine.setParameters (readEngineParameters());

    updateHostTempo();
    loopPlayer.process (buffer.getNumSamples(), getEffectiveBpm(), engine);

    engine.process (midiMessages, outputs, buffer.getNumSamples(), isBypassed());
}

bool DrumforgeAudioProcessor::isBypassed() const
{
    return apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

juce::AudioProcessorParameter* DrumforgeAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void DrumforgeAudioProcessor::timerCallback()
{
    reapRetiredKits();
    reapRetiredLoops();
}

void DrumforgeAudioProcessor::updateHostTempo()
{
    auto* head = getPlayHead();

    if (head == nullptr)
        return;

    if (const auto position = head->getPosition())
        if (const auto bpm = position->getBpm())
            hostBpm.store (juce::jlimit (20.0, 400.0, *bpm), std::memory_order_relaxed);
}

void DrumforgeAudioProcessor::reapRetiredLoops()
{
    const int accepted = loopPlayer.activeToken();

    retiredLoops.erase (std::remove_if (retiredLoops.begin(), retiredLoops.end(),
                                        [accepted] (const RetiredLoop& entry)
                                        {
                                            return entry.token <= accepted;
                                        }),
                        retiredLoops.end());
}

void DrumforgeAudioProcessor::reapRetiredKits()
{
    const int accepted = engine.acceptedKitToken();

    retiredKits.erase (std::remove_if (retiredKits.begin(), retiredKits.end(),
                                       [accepted] (const RetiredKit& entry)
                                       {
                                           return entry.token <= accepted;
                                       }),
                       retiredKits.end());
}

void DrumforgeAudioProcessor::adoptLoadedKit (
    std::unique_ptr<const forja::drumforge::LoadedKit> loaded)
{
    if (loaded == nullptr)
        return;

    auto shared = std::shared_ptr<const forja::drumforge::LoadedKit> (loaded.release());
    const int token = ++kitToken;

    if (activeKit != nullptr)
        retiredKits.push_back ({ activeKit, token });

    activeKit = shared;
    lastKitFile = juce::File (shared->kit.sourceFile);

    engine.requestKit (activeKit.get(), token);

    apvts.state.setProperty ("kitPath", shared->kit.sourceFile, nullptr);
    apvts.state.setProperty ("kitName", shared->kit.name, nullptr);

    // O audio confirma a troca pelo token; so o kit ja aceito pode ser liberado.
    // Descartar por tamanho liberaria um kit que a DAW parada ainda referencia.
    reapRetiredKits();
}

void DrumforgeAudioProcessor::startLoad (forja::drumforge::Kit kit, bool applyMix)
{
    if (kit.sampleCount() == 0)
    {
        kitStatus = "O kit nao tem samples.";
        ++kitRevision;
        return;
    }

    reapRetiredKits();

    // Carga manual: o kit premixado assume os knobs; sem mix no arquivo, a mesa
    // volta ao neutro para o pad nao herdar o mix do kit anterior.
    if (applyMix)
        applyMixerFromKit (kit);

    kitStatus = "Carregando " + kit.name + "...";

    const auto guarded = aliveFlag;

    store.loadAsync (std::move (kit),
                     [this, guarded] (std::unique_ptr<const forja::drumforge::LoadedKit> loaded,
                                      juce::String error)
                     {
                         if (guarded == nullptr
                             || ! guarded->load (std::memory_order_acquire))
                         {
                             return;
                         }

                         if (loaded == nullptr)
                         {
                             kitStatus = error.isNotEmpty() ? error
                                                            : juce::String ("Falha ao carregar o kit.");
                             ++kitRevision;
                             return;
                         }

                         const auto name = loaded->kit.name;
                         adoptLoadedKit (std::move (loaded));
                         kitStatus = error.isNotEmpty()
                                         ? error
                                         : ("Kit " + name + " pronto");
                         ++kitRevision;
                     });
}

void DrumforgeAudioProcessor::applyMixerFromKit (const forja::drumforge::Kit& kit)
{
    auto set = [this] (const juce::String& id, float value)
               {
                   if (auto* parameter = apvts.getParameter (id))
                       parameter->setValueNotifyingHost (value);
               };

    if (! kit.hasMix)
    {
        // Mesa neutra: os mesmos defaults do createParameterLayout.
        for (int pad = 0; pad < kNumPads; ++pad)
        {
            set (padId (pad, "trim"), 0.50f);
            set (padId (pad, "fader"), 0.91f);
            set (padId (pad, "pan"), 0.50f);
            set (padId (pad, "hpf"), 0.00f);
            set (padId (pad, "eqLow"), 0.50f);
            set (padId (pad, "eqMid"), 0.50f);
            set (padId (pad, "eqHigh"), 0.50f);
            set (padId (pad, "compThr"), 0.70f);
            set (padId (pad, "compRatio"), 0.10f);
            set (padId (pad, "send1"), 0.00f);
            set (padId (pad, "send2"), 0.00f);
        }

        set ("master_trim", 0.50f);
        set ("master_comp", 0.00f);
        set ("master_fx1Regen", 0.50f);
        set ("master_fx1Position", 1.00f);
        set ("master_fx2Drive", 0.20f);
        return;
    }

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto& mix = kit.mixers[(size_t) pad];

        set (padId (pad, "trim"), mix.trim);
        set (padId (pad, "fader"), mix.fader);
        set (padId (pad, "pan"), mix.pan);
        set (padId (pad, "hpf"), mix.hpf);
        set (padId (pad, "eqLow"), mix.eqLow);
        set (padId (pad, "eqMid"), mix.eqMid);
        set (padId (pad, "eqHigh"), mix.eqHigh);
        set (padId (pad, "compThr"), mix.compThr);
        set (padId (pad, "compRatio"), mix.compRatio);
        set (padId (pad, "send1"), mix.send1);
        set (padId (pad, "send2"), mix.send2);
    }

    const auto& master = kit.master;

    set ("master_trim", master.trim);
    set ("master_comp", master.comp);
    set ("master_fx1Regen", master.fx1Regen);
    set ("master_fx1Position", master.fx1Position);
    set ("master_fx2Drive", master.fx2Drive);
}

void DrumforgeAudioProcessor::loadKitFileAsync (const juce::File& kitFile, bool applyMix)
{
    forja::drumforge::Kit kit;
    juce::String error;

    if (! forja::drumforge::Kit::loadFromFile (kitFile, kit, error))
    {
        kitStatus = error.isNotEmpty() ? error
                                       : ("Kit invalido: " + kitFile.getFileName());
        ++kitRevision;
        return;
    }

    startLoad (std::move (kit), applyMix);
}

void DrumforgeAudioProcessor::importKitFolderAsync (const juce::File& folder, bool applyMix)
{
    juce::String error;
    auto kit = forja::drumforge::Kit::createFromSampleFolder (folder, error);

    if (kit.sampleCount() == 0)
    {
        kitStatus = error.isNotEmpty() ? error : juce::String ("Pasta sem samples.");
        ++kitRevision;
        return;
    }

    const auto library = getLibraryDirectory();

    if (library.createDirectory().failed())
    {
        kitStatus = "Nao foi possivel criar " + library.getFullPathName();
        ++kitRevision;
        return;
    }

    const auto target = library.getChildFile (
        juce::File::createLegalFileName (folder.getFileName()) + ".dfkit");

    juce::String saveError;

    if (kit.saveToFile (target, saveError))
        kit.sourceFile = target.getFullPathName();
    else
        kit.sourceFile.clear();

    startLoad (std::move (kit), applyMix);
}

void DrumforgeAudioProcessor::importDroppedFile (const juce::File& dropped, bool applyMix)
{
    if (dropped.isDirectory())
    {
        importKitFolderAsync (dropped, applyMix);
        return;
    }

    loadKitFileAsync (dropped, applyMix);
}

std::shared_ptr<const forja::drumforge::LoadedKit> DrumforgeAudioProcessor::getLoadedKit() const
{
    return activeKit;
}

forja::drumforge::Kit DrumforgeAudioProcessor::getKitCopy() const
{
    return activeKit != nullptr ? activeKit->kit : fallbackKit;
}

forja::drumforge::Kit DrumforgeAudioProcessor::getKitForSaving() const
{
    auto kit = getKitCopy();

    auto get = [this] (const juce::String& id, float& target)
               {
                   if (auto* parameter = apvts.getParameter (id))
                       target = parameter->getValue();
               };

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        auto& mix = kit.mixers[(size_t) pad];

        get (padId (pad, "trim"), mix.trim);
        get (padId (pad, "fader"), mix.fader);
        get (padId (pad, "pan"), mix.pan);
        get (padId (pad, "hpf"), mix.hpf);
        get (padId (pad, "eqLow"), mix.eqLow);
        get (padId (pad, "eqMid"), mix.eqMid);
        get (padId (pad, "eqHigh"), mix.eqHigh);
        get (padId (pad, "compThr"), mix.compThr);
        get (padId (pad, "compRatio"), mix.compRatio);
        get (padId (pad, "send1"), mix.send1);
        get (padId (pad, "send2"), mix.send2);
    }

    get ("master_trim", kit.master.trim);
    get ("master_comp", kit.master.comp);
    get ("master_fx1Regen", kit.master.fx1Regen);
    get ("master_fx1Position", kit.master.fx1Position);
    get ("master_fx2Drive", kit.master.fx2Drive);

    kit.hasMix = true;
    return kit;
}

void DrumforgeAudioProcessor::markKitSaved (const juce::File& file)
{
    kitStatus = "Kit salvo em " + file.getFileName();
    ++kitRevision;
}

juce::String DrumforgeAudioProcessor::getPadName (int padIndex) const
{
    if (! juce::isPositiveAndBelow (padIndex, kNumPads))
        return {};

    const auto& pads = activeKit != nullptr ? activeKit->kit.pads : fallbackKit.pads;
    const auto& name = pads[(size_t) padIndex].name;

    return name.isNotEmpty() ? name : ("PAD " + juce::String (padIndex + 1));
}

juce::String DrumforgeAudioProcessor::getKitStatus() const
{
    return kitStatus;
}

int DrumforgeAudioProcessor::getLoadProgress() const
{
    return store.getProgress();
}

bool DrumforgeAudioProcessor::isLoadingKit() const
{
    return store.isLoading();
}

juce::File DrumforgeAudioProcessor::getLibraryDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Forja")
        .getChildFile ("Drumforge")
        .getChildFile ("Kits");
}

namespace
{

// O drumlabooh instala os kits do drum_sklad em pastas proprias da conta. Ler
// essas raizes aqui dispensa copiar seja o que for para o Drumforge.
juce::Array<juce::File> txtKitRoots()
{
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    juce::Array<juce::File> roots = {
        home.getChildFile ("drum_sklad"),
        home.getChildFile ("drumlabooh-kits"),
        home.getChildFile ("drumrox-kits")
    };

    roots.removeIf ([] (const juce::File& root) { return ! root.isDirectory(); });
    return roots;
}

void addTxtKitsBelow (const juce::File& root, juce::Array<juce::File>& kits)
{
    for (const auto& child : root.findChildFiles (juce::File::findDirectories, false))
    {
        if (forja::drumforge::Kit::folderHasTxtKit (child))
        {
            kits.addIfNotAlreadyThere (child);
            continue;
        }

        // Arquivo baixado do GitHub costuma trazer um nivel a mais (drum_sklad-main).
        for (const auto& inside : child.findChildFiles (juce::File::findDirectories, false))
            if (forja::drumforge::Kit::folderHasTxtKit (inside))
                kits.addIfNotAlreadyThere (inside);
    }
}

} // namespace

juce::Array<juce::File> DrumforgeAudioProcessor::findLibraryKits() const
{
    auto kits = getLibraryDirectory().findChildFiles (juce::File::findFiles, true, "*.dfkit");

    for (const auto& root : txtKitRoots())
        addTxtKitsBelow (root, kits);

    kits.sort();
    return kits;
}

juce::File DrumforgeAudioProcessor::getLoopDirectory() const
{
    return forja::drumforge::loops::libraryDirectory();
}

std::vector<forja::drumforge::loops::Entry> DrumforgeAudioProcessor::findLoops() const
{
    const auto directory = getLoopDirectory();

    if (! directory.isDirectory())
        return {};

    return forja::drumforge::loops::scan (directory);
}

bool DrumforgeAudioProcessor::loadLoopFile (const juce::File& loopFile)
{
    auto sequence = std::make_shared<forja::drumforge::loops::Sequence>();
    juce::String error;

    if (! forja::drumforge::loops::readSequence (loopFile, *sequence, error))
    {
        loopStatus = error.isNotEmpty() ? error
                                        : ("Loop invalido: " + loopFile.getFileName());
        return false;
    }

    // A sequencia fica viva em previewLoop; o player so solta o ponteiro antigo
    // quando o audio confirma o token novo.
    const int token = ++loopToken;

    if (previewLoop != nullptr)
        retiredLoops.push_back ({ previewLoop, token });

    previewLoop = sequence;
    loopPlayer.request (previewLoop.get(), token);

    // Mesma regra do kit: o timer libera o loop antigo so depois que o audio
    // adota o token novo, mesmo que a DAW pare de processar por um tempo.
    reapRetiredLoops();

    apvts.state.setProperty ("loopPath", loopFile.getFullPathName(), nullptr);
    apvts.state.setProperty ("loopName", sequence->name, nullptr);

    loopStatus = sequence->name + " - " + juce::String ((int) sequence->hits.size())
               + " golpes - " + juce::String (sequence->bpm, 0) + " BPM";

    return true;
}

juce::File DrumforgeAudioProcessor::importLoopFile (const juce::File& loopFile)
{
    const auto target = forja::drumforge::loops::importFile (loopFile, getLoopDirectory());

    if (! target.existsAsFile())
    {
        loopStatus = "Nao foi possivel importar " + loopFile.getFileName();
        return {};
    }

    loadLoopFile (target);
    return target;
}

int DrumforgeAudioProcessor::importLoopFolder (const juce::File& folder)
{
    const auto copied = forja::drumforge::loops::importFolder (folder, getLoopDirectory());

    if (copied == 0)
        loopStatus = "Nao foi possivel importar " + folder.getFileName();
    else
        loopStatus = folder.getFileName() + " - " + juce::String (copied)
                   + (copied == 1 ? " loop importado" : " loops importados");

    return copied;
}

juce::String DrumforgeAudioProcessor::getSelectedLoopPath() const
{
    return previewLoop != nullptr ? previewLoop->file : juce::String();
}

juce::String DrumforgeAudioProcessor::getSelectedLoopName() const
{
    return previewLoop != nullptr ? previewLoop->name : juce::String();
}

std::shared_ptr<const forja::drumforge::loops::Sequence> DrumforgeAudioProcessor::getPreviewLoop() const
{
    return previewLoop;
}

void DrumforgeAudioProcessor::startLoopPreview()
{
    if (previewLoop == nullptr)
    {
        loopStatus = "Selecione um loop da biblioteca.";
        return;
    }

    loopPlayer.start();
}

void DrumforgeAudioProcessor::stopLoopPreview()
{
    loopPlayer.stop();
}

bool DrumforgeAudioProcessor::isLoopPreviewing() const
{
    return loopPlayer.isPlaying();
}

double DrumforgeAudioProcessor::getLoopPositionBeats() const
{
    return loopPlayer.positionBeats();
}

double DrumforgeAudioProcessor::getEffectiveBpm() const
{
    const auto* sync = apvts.getRawParameterValue ("loop_sync");

    if (sync != nullptr && sync->load() > 0.5f)
        return juce::jlimit (20.0, 400.0, hostBpm.load (std::memory_order_relaxed));

    if (const auto* bpm = apvts.getRawParameterValue ("loop_bpm"))
        return 40.0 + 200.0 * (double) bpm->load();

    return 120.0;
}

void DrumforgeAudioProcessor::previewPad (int padIndex, float velocity)
{
    engine.pushTrigger (padIndex, velocity);
}

std::array<int, 128> DrumforgeAudioProcessor::noteToPadMap() const
{
    std::array<int, 128> map {};
    map.fill (-1);

    auto loaded = getLoadedKit();

    if (loaded == nullptr)
    {
        // Sem kit carregado vale o mapa padrao, o mesmo que a aba Kits mostra.
        const auto kit = getKitCopy();

        for (int pad = 0; pad < kNumPads; ++pad)
        {
            const int note = kit.pads[(size_t) pad].note;

            if (juce::isPositiveAndBelow (note, 128))
                map[(size_t) note] = pad;
        }

        return map;
    }

    // Sem nenhum sample carregado a grade ainda mostra o mapa de notas do kit,
    // para o usuario ver quais pads vao tocar quando importar as amostras.
    const bool anyAudio = loaded->padsWithAudio > 0;

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        if (anyAudio && loaded->layers[(size_t) pad].empty())
            continue;

        const int note = loaded->kit.pads[(size_t) pad].note;

        if (juce::isPositiveAndBelow (note, 128))
            map[(size_t) note] = pad;
    }

    return map;
}

void DrumforgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (activeKit != nullptr)
    {
        state.setProperty ("kitPath", activeKit->kit.sourceFile, nullptr);
        state.setProperty ("kitName", activeKit->kit.name, nullptr);
    }

    if (previewLoop != nullptr)
    {
        state.setProperty ("loopPath", previewLoop->file, nullptr);
        state.setProperty ("loopName", previewLoop->name, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrumforgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    const auto path = apvts.state.getProperty ("kitPath").toString();

    // Restauracao do host: so reabre o kit pelos samples; a mix que o host salvou
    // vence a mix embutida no arquivo (applyMix = false).
    if (path.isNotEmpty())
        loadKitFileAsync (juce::File (path), false);

    const auto loopPath = apvts.state.getProperty ("loopPath").toString();

    if (loopPath.isNotEmpty() && juce::File (loopPath).existsAsFile())
        loadLoopFile (juce::File (loopPath));
}

juce::AudioProcessorEditor* DrumforgeAudioProcessor::createEditor()
{
    return new DrumforgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumforgeAudioProcessor();
}
