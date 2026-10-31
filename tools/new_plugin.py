#!/usr/bin/env python3
"""Scaffold a new Forja plugin from tools/templates/plugin.

The generated code is a compiling pass-through with one Forja knob, bypass,
meters, a FUNDAMENTOS placeholder and a README skeleton. The DSP TODO in the
adapter must be replaced by the literal Airwindows transcription before the
plugin is considered done (see docs/NOVO-PLUGIN.md).

Example:
    python tools/new_plugin.py --family OneKnob --product Areia --code Area \
        --slug areia --model Areia --airwindows PurestAir --family-label "DE MIX" \
        --subtitle "AMACIADOR" --status "Amaciador de picos" --footer "DSP Airwindows" \
        --categories Fx --company "Forja"
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TEMPLATES = ROOT / "tools" / "templates" / "plugin"
AIRWINDOWS = ROOT / "Airwindows"

PLACEHOLDER = re.compile(r"@@([A-Z_]+)@@")


def ident(value: str, label: str) -> str:
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", value):
        sys.exit(f"{label} must be an ASCII identifier (letters and digits): {value!r}")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a Forja plugin scaffold.")
    parser.add_argument("--family", required=True, help="family folder under Airwindows/, e.g. Eq, Tape, OneKnob")
    parser.add_argument("--product", required=True, help="product name as shown in the DAW")
    parser.add_argument("--code", required=True, help="4-character VST3 plugin code")
    parser.add_argument("--slug", default="", help="lowercase id used in bundles/files")
    parser.add_argument("--model", default="", help="Airwindows model name (C++ identifier)")
    parser.add_argument("--bundle", default="", help="bundle id (default com.forja.<slug>)")
    parser.add_argument("--categories", default="Fx", help="VST3 categories")
    parser.add_argument("--company", default="Forja", help="COMPANY_NAME (family label)")
    parser.add_argument("--family-label", default="", help="top bar family, e.g. 'DE MIX'")
    parser.add_argument("--subtitle", default="PROCESSADOR", help="top bar subtitle")
    parser.add_argument("--status", default="Processador Forja", help="status bar subtitle")
    parser.add_argument("--footer", default="DSP Airwindows \u00b7 MIT", help="status bar footer")
    parser.add_argument("--airwindows", default="", help="Airwindows model display name")
    parser.add_argument("--dry-run", action="store_true", help="print files without writing")
    args = parser.parse_args()

    product = ident(args.product, "--product")
    slug = args.slug or product.lower()
    if not re.fullmatch(r"[a-z][a-z0-9]*", slug):
        sys.exit(f"--slug must be lowercase ASCII: {slug!r}")
    model = ident(args.model or product, "--model")

    code = args.code
    if not re.fullmatch(r"[A-Za-z0-9]{4}", code):
        sys.exit("--code must have exactly 4 letters/digits (VST3 plugin code)")

    family = ident(args.family.strip(), "--family")
    family_dir = AIRWINDOWS / family
    if not family_dir.is_dir():
        sys.exit(f"family folder not found: {family_dir}")

    airwindows = args.airwindows or model
    values = {
        "PRODUCT": product,
        "PREFIX": product,
        "SLUG": slug,
        "MODEL": model,
        "CODE": code,
        "BUNDLE": args.bundle or f"com.forja.{slug}",
        "CATEGORIES": args.categories,
        "COMPANY": args.company,
        "FAMILY": family,
        "FAMILY_LABEL": args.family_label or family.upper(),
        "SUBTITLE": args.subtitle,
        "STATUS_SUBTITLE": args.status,
        "FOOTER": args.footer,
        "AIRWINDOWS": airwindows,
        "PARAM_NAME": "Quantidade",
    }

    plugin_dir = family_dir / product
    if plugin_dir.exists():
        sys.exit(f"refusing to overwrite existing plugin folder: {plugin_dir}")

    adapter_path = ROOT / "Forja" / "Source" / "forja" / "dsp" / f"Forja{model}.h"
    if adapter_path.exists():
        sys.exit(f"refusing to overwrite existing adapter: {adapter_path}")

    def render(text: str) -> str:
        def substitute(match: re.Match[str]) -> str:
            key = match.group(1)
            if key not in values:
                sys.exit(f"unknown placeholder @@{key}@@ in template")
            return values[key]

        return PLACEHOLDER.sub(substitute, text)

    writes: list[tuple[pathlib.Path, str]] = []

    for source in sorted(TEMPLATES.rglob("*")):
        if not source.is_file():
            continue

        relative = source.relative_to(TEMPLATES)

        if relative.name == "ForjaDsp.h":
            destination = adapter_path
        else:
            destination = plugin_dir / relative

        writes.append((destination, render(source.read_text(encoding="utf-8"))))

    for destination, content in writes:
        print(f"{'would write' if args.dry_run else 'write'} {destination.relative_to(ROOT)}")
        if not args.dry_run:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(content, encoding="utf-8", newline="\n")

    family_cmake = family_dir / "CMakeLists.txt"
    add_line = f"add_subdirectory({product})"

    if not args.dry_run:
        if family_cmake.exists():
            text = family_cmake.read_text(encoding="utf-8")
        else:
            text = ""

        if add_line not in text:
            if text and not text.endswith("\n"):
                text += "\n"
            text += add_line + "\n"
            family_cmake.write_text(text, encoding="utf-8", newline="\n")
            print(f"write {family_cmake.relative_to(ROOT)} (+ {add_line})")
        else:
            print(f"kept {family_cmake.relative_to(ROOT)} (already lists {product})")

        group_cmake = AIRWINDOWS / "CMakeLists.txt"
        group_text = group_cmake.read_text(encoding="utf-8")
        family_line = f"add_subdirectory({family})"
        if family_line not in group_text.splitlines():
            if group_text and not group_text.endswith("\n"):
                group_text += "\n"
            group_text += family_line + "\n"
            group_cmake.write_text(group_text, encoding="utf-8", newline="\n")
            print(f"write {group_cmake.relative_to(ROOT)} (+ {family_line})")
        else:
            print(f"kept {group_cmake.relative_to(ROOT)} (already lists {family})")

    print()
    print("Next steps (docs/NOVO-PLUGIN.md):")
    print(f"  1. Vendorize Airwindows {airwindows} into Forja/Source/forja/vendor/airwindows/{model}/")
    print(f"  2. Port processDoubleReplacing into {adapter_path.relative_to(ROOT)}")
    print(f"  3. Replace the parameter/UI placeholders in {plugin_dir.relative_to(ROOT)}/Source")
    print(f"  4. Add Tests/Source/parity_{slug}.cpp and wire it in Tests/CMakeLists.txt")
    print(f"  5. Add the adapter to Tests/Source/alloc_guard.cpp")
    print(f"  6. cp cmake configure + build {product}_VST3 {product}_Standalone and run the tests")
    print(f"  7. Fill README.md and the FUNDAMENTOS, update the catalog and the suite README")
    print()
    print(f"Then: cmake -S . -B build && cmake --build build --config Release --target {product}_VST3 {product}_Standalone")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
