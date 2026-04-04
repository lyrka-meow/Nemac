#!/bin/bash
set -e

REPO="lyrka-meow/Nemac"
VERSION="${1:-$(date +v%Y.%m.%d)}"

echo "=== Nemac сборка ${VERSION} ==="

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

mkdir -p "$PROJECT_DIR/build"
cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$PROJECT_DIR/build" -j"$(nproc)"

BINARY="$PROJECT_DIR/build/nemac"

if [ ! -f "$BINARY" ]; then
    echo "Ошибка: бинарник не найден"
    exit 1
fi

strip "$BINARY"
echo "Бинарник: $BINARY ($(du -h "$BINARY" | cut -f1))"

if command -v gh &>/dev/null; then
    echo "Создание релиза ${VERSION}..."
    gh release create "$VERSION" "$BINARY" \
        --repo "$REPO" \
        --title "Nemac ${VERSION}" \
        --notes "Автосборка ${VERSION}" \
        --latest
    echo "Релиз ${VERSION} создан"
else
    echo "gh CLI не установлен. Установите: pacman -S github-cli"
    echo "Затем: gh auth login"
fi
