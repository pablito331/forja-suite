#!/usr/bin/env bash
# Script de instalacao automatica dos plugins Forja no Linux
set -euo pipefail

DEST_DIR="${HOME}/.vst3"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="${SCRIPT_DIR}/VST3"

echo "=== Instalador Forja (Linux VST3) ==="

if [ ! -d "$SOURCE_DIR" ]; then
    echo "Erro: Diretorio VST3 nao encontrado em $SCRIPT_DIR"
    exit 1
fi

# Cria diretorio de destino padrao do usuario se nao existir
mkdir -p "$DEST_DIR"

# Cria estrutura de pastas do usuario em ~/Documents/Forja
DOCS_DIR="${HOME}/Documents/Forja"
mkdir -p "${DOCS_DIR}/Canivete"
mkdir -p "${DOCS_DIR}/Drumforge/Kits"
mkdir -p "${DOCS_DIR}/Drumforge/Loops"
mkdir -p "${DOCS_DIR}/Pianoforge"
mkdir -p "${DOCS_DIR}/Rack"

echo "Instalando plugins em: $DEST_DIR"

count=0
for plugin in "$SOURCE_DIR"/*.vst3; do
    if [ -d "$plugin" ]; then
        name="$(basename "$plugin")"
        echo "  -> Copiando $name..."
        cp -R "$plugin" "$DEST_DIR/"
        count=$((count + 1))
    fi
done

echo ""
echo "Sucesso! $count plugin(s) instalado(s) em $DEST_DIR."
echo "Estrutura de dados criada em: $DOCS_DIR"
echo "Abra ou reinicie sua DAW para escanear os novos plugins."
