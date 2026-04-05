#!/bin/bash
set -e

REPO="lyrka-meow/Nemac"
INSTALL_DIR="/usr/local/bin"
DATA_DIR="/usr/share/nemac"
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

banner() {
    echo -e "${CYAN}${BOLD}"
    echo "  _   _                            "
    echo " | \ | | ___ _ __ ___   __ _  ___  "
    echo " |  \| |/ _ \ '_ \` _ \ / _\` |/ __| "
    echo " | |\  |  __/ | | | | | (_| | (__  "
    echo " |_| \_|\___|_| |_| |_|\__,_|\___| "
    echo ""
    echo -e "${NC}"
}

check_arch() {
    if [ ! -f /etc/arch-release ]; then
        echo -e "${RED}Nemac поддерживает только Arch Linux${NC}"
        exit 1
    fi
}

install_deps() {
    echo -e "${CYAN}Установка зависимостей...${NC}"
    sudo pacman -S --needed --noconfirm \
        xorg-server xorg-xinit xorg-xrandr \
        libx11 libxcomposite libxdamage libxfixes libxinerama libxft \
        glew mesa imlib2 dbus \
        qt6-base qt6-declarative \
        curl git base-devel cmake
}

install_from_release() {
    echo -e "${CYAN}Загрузка последнего релиза...${NC}"
    local url
    url=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" \
        | grep '"browser_download_url"' | head -1 | cut -d'"' -f4)

    if [ -z "$url" ]; then
        echo -e "${RED}Релиз не найден. Попробуйте собрать из исходников.${NC}"
        return 1
    fi

    local tmp="/tmp/nemac-release"
    curl -fsSL -o "$tmp" "$url"
    chmod +x "$tmp"
    sudo cp "$tmp" "${INSTALL_DIR}/nemac"
    rm -f "$tmp"
    echo -e "${GREEN}Бинарник установлен в ${INSTALL_DIR}/nemac${NC}"
}

install_from_source() {
    echo -e "${CYAN}Сборка из исходников...${NC}"
    local build_dir="/tmp/nemac-build"
    rm -rf "$build_dir"
    git clone "https://github.com/${REPO}.git" "$build_dir"
    mkdir -p "$build_dir/build"
    cmake -S "$build_dir" -B "$build_dir/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build_dir/build" -j"$(nproc)"
    sudo cp "$build_dir/build/nemac" "${INSTALL_DIR}/nemac"
    sudo cp "$build_dir/build/panel/nemac-panel" "${INSTALL_DIR}/nemac-panel"
    rm -rf "$build_dir"
    echo -e "${GREEN}Собрано и установлено в ${INSTALL_DIR}${NC}"
}

install_assets() {
    echo -e "${CYAN}Установка ресурсов...${NC}"
    sudo mkdir -p "${DATA_DIR}/wallpapers"
    local walls_url="https://raw.githubusercontent.com/${REPO}/main/assets/wallpapers"
    for f in wall_01.jpg wall_02.jpg wall_03.jpg; do
        if [ ! -f "${DATA_DIR}/wallpapers/$f" ]; then
            sudo curl -fsSL -o "${DATA_DIR}/wallpapers/$f" "${walls_url}/$f" 2>/dev/null || true
        fi
    done
}

setup_xinitrc() {
    local xinitrc="$HOME/.xinitrc"
    if [ -f "$xinitrc" ] && grep -q "nemac" "$xinitrc"; then
        echo -e "${GREEN}.xinitrc уже настроен${NC}"
        return
    fi

    if [ -f "$xinitrc" ]; then
        cp "$xinitrc" "${xinitrc}.bak"
        echo -e "${CYAN}Бэкап .xinitrc -> .xinitrc.bak${NC}"
    fi

    cat > "$xinitrc" << 'EOF'
#!/bin/sh
xrdb -merge ~/.Xresources 2>/dev/null
xset s off -dpms 2>/dev/null
exec nemac
EOF
    chmod +x "$xinitrc"
    echo -e "${GREEN}.xinitrc настроен. Используйте startx для запуска.${NC}"
}

main() {
    banner
    check_arch

    echo -e "${BOLD}Выберите способ установки:${NC}"
    echo "  1) Из релиза (готовый бинарник)"
    echo "  0) Из исходников (сборка)"
    echo ""
    read -rp "Выбор [1/0]: " choice

    install_deps

    case "$choice" in
        0) install_from_source ;;
        *) install_from_release ;;
    esac

    install_assets
    setup_xinitrc

    echo ""
    echo -e "${GREEN}${BOLD}Nemac установлен!${NC}"
    echo -e "Запуск: ${CYAN}startx${NC}"
}

main "$@"
