#!/bin/bash

REPO="lyrka-meow/Nemac"
INSTALL_DIR="/usr/local/bin"
DATA_DIR="/usr/share/nemac"
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME=$(eval echo "~${REAL_USER}")

die() {
    echo -e "${RED}$1${NC}" >&2
    exit 1
}

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

check_root() {
    [ "$(id -u)" -eq 0 ] || die "Запустите с sudo"
}

check_arch() {
    [ -f /etc/arch-release ] || die "Nemac поддерживает только Arch Linux"
}

install_deps() {
    echo -e "${CYAN}Установка зависимостей...${NC}"
    if ! pacman -S --needed --noconfirm \
        xorg-server xorg-xinit xorg-xrandr \
        libx11 libxcomposite libxdamage libxfixes libxinerama libxft \
        glew mesa imlib2 \
        curl > /dev/null 2>&1; then
        die "Не удалось установить зависимости. Проверьте зеркала pacman"
    fi
    echo -e "${GREEN}Зависимости установлены${NC}"
}

install_from_release() {
    echo -e "${CYAN}Загрузка Nemac...${NC}"
    local api_resp
    api_resp=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" 2>/dev/null) || true

    local nemac_url
    nemac_url=$(echo "$api_resp" | grep '"browser_download_url".*nemac"' | head -1 | cut -d'"' -f4)

    if [ -z "$nemac_url" ]; then
        die "Релиз не найден. Проверьте https://github.com/${REPO}/releases"
    fi

    if ! curl -fsSL -o /tmp/nemac "$nemac_url"; then
        die "Не удалось скачать nemac"
    fi
    chmod +x /tmp/nemac
    cp /tmp/nemac "${INSTALL_DIR}/nemac"
    rm -f /tmp/nemac
    echo -e "${GREEN}Nemac установлен${NC}"
}

setup_dirs() {
    mkdir -p "${DATA_DIR}/wallpapers"
    mkdir -p "${REAL_HOME}/.local/share/nemac/wallpapers"
    chown -R "${REAL_USER}:${REAL_USER}" "${REAL_HOME}/.local/share/nemac"
}

setup_xinitrc() {
    local xinitrc="${REAL_HOME}/.xinitrc"
    if [ -f "$xinitrc" ] && grep -q "nemac" "$xinitrc"; then
        return
    fi

    if [ -f "$xinitrc" ]; then
        cp "$xinitrc" "${xinitrc}.bak"
    fi

    cat > "$xinitrc" << 'EOF'
#!/bin/sh
xrdb -merge ~/.Xresources 2>/dev/null
xset s off -dpms 2>/dev/null
exec nemac
EOF
    chmod +x "$xinitrc"
    chown "${REAL_USER}:${REAL_USER}" "$xinitrc"
}

main() {
    banner
    check_root
    check_arch
    install_deps
    install_from_release
    setup_dirs
    setup_xinitrc
    echo ""
    echo -e "${GREEN}${BOLD}Nemac установлен!${NC}"
    echo -e "Запуск: ${CYAN}startx${NC}"
}

main "$@"
