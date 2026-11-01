#include "ModuleCatalog.h"

#include <cstring>
#include <vector>

namespace forjarack
{

namespace
{

// Paleta das familias: cada gaveta herda a cor da sua categoria. Saindo do
// padrao ember unico da suite, mas dentro da mesma temperatura de console.
constexpr std::uint32_t kAccentEmpty = 0xff6b7280;    // grafite (slot cego)
constexpr std::uint32_t kAccentColor = 0xffe0a03c;    // amber (calor / docura)
constexpr std::uint32_t kAccentDrive = 0xffff5a3c;    // laranja quente (saturacao)
constexpr std::uint32_t kAccentDynamics = 0xff6fc58a; // verde (compressao / limite)
constexpr std::uint32_t kAccentEq = 0xff4fc3e0;       // ciano (equalizacao)
constexpr std::uint32_t kAccentSpace = 0xff9a6fe0;    // violeta (ambiente / delay)
constexpr std::uint32_t kAccentLofi = 0xffc9d15a;     // acido (degradacao digital)
constexpr std::uint32_t kAccentInstrument = 0xffe06fa8; // rosa (instrumentos geradores)

// Fabricas dos built-in. Captureless lambdas viram Function pointer do spec.
template <typename Module>
std::unique_ptr<ModuleBase> createModule()
{
    return std::make_unique<Module>();
}

// Registro vivo. Os built-in sao os primeiros, sempre; expansoes se acrescentam
// ao fim via registerModule (a ordem e o indice gravado nos presets).
std::vector<ModuleSpec>& registry()
{
    static std::vector<ModuleSpec> table = []
    {
        std::vector<ModuleSpec> builtins;

        auto add = [&builtins] (ModuleSpec spec) { builtins.push_back (spec); };

        add ({ "vazio", "VAZIO", ModuleCategory::color, kAccentEmpty,
               ModuleRole::effect, 0, { { } }, nullptr, ModuleFace::generic });

        // std::array exige dois niveis de chaves (MSVC nao faz elisao):
        // { { {label,default}, ... } } -> array + C array + KnobSpec.
        add ({ "cera", "CERA", ModuleCategory::color, kAccentColor,
               ModuleRole::effect, 1,
               { { { "CALOR", 0.5f } } }, createModule<CeraModule>, ModuleFace::warmth });

        add ({ "lixa", "LIXA", ModuleCategory::drive, kAccentDrive,
               ModuleRole::effect, 1,
               { { { "DRIVE", 0.0f } } }, createModule<LixaModule>, ModuleFace::drive });

        add ({ "mel", "MEL", ModuleCategory::color, kAccentColor,
               ModuleRole::effect, 1,
               { { { "DOCE", 0.0f } } }, createModule<MelModule>, ModuleFace::warmth });

        add ({ "tempera", "TEMPERA", ModuleCategory::dynamics, kAccentDynamics,
               ModuleRole::effect, 3,
               { { { "COMP", 0.0f }, { "SAIDA", 0.5f }, { "MIX", 1.0f } } },
               createModule<TemperaModule>, ModuleFace::dynamics, true });

        add ({ "muralha", "MURALHA", ModuleCategory::dynamics, kAccentDynamics,
               ModuleRole::effect, 5,
               { { { "BOOST", 0.0f }, { "MATCH", 0.0f }, { "RUIDO", 0.7f },
                     { "TETO", 0.75f }, { "MODO", 0.0f } } },
               createModule<MuralhaModule>, ModuleFace::dynamics, true });

        add ({ "cinzel", "CINZEL", ModuleCategory::eq, kAccentEq,
               ModuleRole::effect, 6,
               { { { "AGUDO", 0.5f }, { "ALTO MED", 0.5f }, { "BAIXO MED", 0.5f },
                   { "FREQ AG", 0.5f }, { "FREQ BM", 0.5f }, { "MIX", 1.0f } } },
               createModule<CinzelModule>, ModuleFace::eq });

        add ({ "oxido", "OXIDO", ModuleCategory::drive, kAccentDrive,
               ModuleRole::effect, 7,
               { { { "ENTRADA", 0.5f }, { "AG FITA", 0.5623413f }, { "GR FITA", 0.5623413f },
                   { "FLUTTER", 0.5f }, { "RUIDO", 0.5f }, { "SAIDA", 0.5f },
                   { "DRY/WET", 1.0f } } },
               createModule<OxidoModule>, ModuleFace::tape });

        add ({ "folego", "FOLEGO", ModuleCategory::space, kAccentSpace,
               ModuleRole::effect, 6,
               { { { "REGEN", 0.5f }, { "DEREZ", 0.5f }, { "FILTRO", 0.5f },
                   { "EARLY", 0.5f }, { "POSICAO", 0.9f }, { "DRY/WET", 0.5f } } },
               createModule<FolegoModule>, ModuleFace::space });

        add ({ "salao", "SALAO", ModuleCategory::space, kAccentSpace,
               ModuleRole::effect, 5,
               { { { "REGEN", 0.5f }, { "POSICAO", 0.75f }, { "DISTANCIA", 0.25f },
                   { "DOWNREZ", 1.0f }, { "DRY/WET", 0.5f } } },
               createModule<SalaoModule>, ModuleFace::space });

        add ({ "repique", "REPIQUE", ModuleCategory::space, kAccentSpace,
               ModuleRole::effect, 6,
               { { { "TEMPO", 1.0f }, { "REGEN", 0.0f }, { "FREQ", 0.5f },
                   { "RESO", 0.0f }, { "FLUTTER", 0.0f }, { "DRY/WET", 1.0f } } },
               createModule<RepiqueModule>, ModuleFace::space });

        // Lote 2 (nucleo de mix). Entram ACRESCENTADOS ao fim: o indice gravado
        // nos presets existentes nao pode mudar.
        add ({ "corpo", "CORPO", ModuleCategory::color, kAccentColor,
               ModuleRole::effect, 1,
               { { { "CORPO", 0.5f } } }, createModule<CorpoModule>, ModuleFace::warmth });

        add ({ "impeto", "IMPETO", ModuleCategory::color, kAccentColor,
               ModuleRole::effect, 1,
               { { { "IMPETO", 0.5f } } }, createModule<ImpetoModule>, ModuleFace::warmth });

        add ({ "rebarba", "REBARBA", ModuleCategory::drive, kAccentDrive,
               ModuleRole::effect, 1,
               { { { "SUAVID", 0.0f } } }, createModule<RebarbaModule>, ModuleFace::drive });

        add ({ "dique", "DIQUE", ModuleCategory::dynamics, kAccentDynamics,
               ModuleRole::effect, 0,
               { { } }, createModule<DiqueModule>, ModuleFace::dynamics, true });

        add ({ "torno", "TORNO", ModuleCategory::eq, kAccentEq,
               ModuleRole::effect, 3,
               { { { "ENTRADA", 0.5f }, { "AGUDOS", 0.5f }, { "GRAVES", 0.5f } } },
               createModule<TornoModule>, ModuleFace::eq });

        add ({ "lima", "LIMA", ModuleCategory::eq, kAccentEq,
               ModuleRole::effect, 8,
               { { { "AGUDO", 0.5f }, { "ALTO MED", 0.5f }, { "BAIXO MED", 0.5f },
                   { "GRAVE", 0.5f }, { "FREQ AG", 0.5f }, { "FREQ AM", 0.5f },
                   { "FREQ BM", 0.5f }, { "FREQ GR", 0.5f } } },
               createModule<LimaModule>, ModuleFace::eq });

        add ({ "malho", "MALHO", ModuleCategory::dynamics, kAccentDynamics,
               ModuleRole::effect, 5,
               { { { "COMP", 0.5f }, { "ATQUE", 0.5f }, { "SOLTURA", 0.5f },
                   { "DRIVE", 0.5f }, { "MIX", 1.0f } } },
               createModule<MalhoModule>, ModuleFace::dynamics, true });

        add ({ "prensa", "PRENSA", ModuleCategory::dynamics, kAccentDynamics,
               ModuleRole::effect, 6,
               { { { "PRESSAO", 0.0f }, { "VELOC", 0.25f }, { "MIADO", 1.0f },
                   { "GARRA", 0.5f }, { "SAIDA", 0.5f }, { "MIX", 1.0f } } },
               createModule<PrensaModule>, ModuleFace::dynamics, true });

        add ({ "escoria", "ESCORIA", ModuleCategory::dynamics, kAccentDynamics,
               ModuleRole::effect, 5,
               { { { "INTENS", 0.0f }, { "VELOC", 0.5f }, { "PROFUND", 0.5f },
                   { "FILTRO", 0.5f }, { "SENSO", 0.0f } } },
               createModule<EscoriaModule>, ModuleFace::dynamics });

        add ({ "laminador", "LAMINADOR", ModuleCategory::drive, kAccentDrive,
               ModuleRole::effect, 9,
               { { { "ENTRADA", 0.5f }, { "INCLINA", 0.5f }, { "FORMA", 0.5f },
                   { "FLUT", 0.5f }, { "VEL FLUT", 0.5f }, { "BIAS", 0.5f },
                   { "CABECOTE", 0.5f }, { "FREQ CAB", 0.5f }, { "SAIDA", 0.5f } } },
               createModule<LaminadorModule>, ModuleFace::tape });

        add ({ "granalha", "GRANALHA", ModuleCategory::lofi, kAccentLofi,
               ModuleRole::effect, 6,
               { { { "TAXA", 1.0f }, { "BITS", 1.0f }, { "QUADRADO", 0.0f },
                   { "BRILHO", 1.0f }, { "GRAVEZA", 0.0f }, { "SAIDA", 1.0f } } },
               createModule<GranalhaModule>, ModuleFace::digital });

        add ({ "eco", "ECO", ModuleCategory::space, kAccentSpace,
               ModuleRole::effect, 4,
               { { { "ESPACO", 0.5f }, { "REGEN", 0.5f }, { "VELOC", 0.5f },
                   { "DRY/WET", 0.5f } } },
               createModule<EcoModule>, ModuleFace::space });

        return builtins;
    }();

    return table;
}

const ModuleSpec& voidSpec()
{
    return registry().front();
}

} // namespace

int ModuleCatalog::count()
{
    return static_cast<int> (registry().size());
}

const ModuleSpec& ModuleCatalog::specByIndex (int index)
{
    if (index < 0 || index >= static_cast<int> (registry().size()))
        return voidSpec();

    return registry()[(size_t) index];
}

int ModuleCatalog::indexOf (const char* id)
{
    for (int i = 0; i < static_cast<int> (registry().size()); ++i)
        if (std::strcmp (registry()[(size_t) i].id, id) == 0)
            return i;

    return -1;
}

bool ModuleCatalog::registerModule (const ModuleSpec& spec)
{
    if (spec.id == nullptr || spec.name == nullptr
        || indexOf (spec.id) >= 0)
        return false;

    registry().push_back (spec);
    return true;
}

std::unique_ptr<ModuleBase> ModuleCatalog::create (int index)
{
    const auto& spec = specByIndex (index);
    return spec.factory != nullptr ? spec.factory() : nullptr;
}

std::array<float, kMaxModuleParams> ModuleCatalog::defaultParams (int index)
{
    std::array<float, kMaxModuleParams> out {};
    const auto& s = specByIndex (index);

    for (int i = 0; i < s.numParams && i < kMaxModuleParams; ++i)
        out[(size_t) i] = s.knobs[(size_t) i].defaultValue;

    return out;
}

} // namespace forjarack
