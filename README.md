# Nemac

Nemac — десктопное окружение для X11 (Arch Linux).

## Установка

```bash
sudo bash -c "$(curl -fsSL https://raw.githubusercontent.com/lyrka-meow/Nemac/master/tools/installer.sh)"
```

## Запуск

```bash
startx
```

## Управление

| Комбинация | Действие |
|---|---|
| `Alt + Enter` | Открыть терминал |
| `Alt + Q` | Закрыть окно |
| `Alt + Shift + Q` | Выход из DE |
| `Alt + P` | Закрепить/открепить окно по центру |
| `Alt + F` | Полный экран |
| `Alt + ЛКМ` | Перемещение окна |
| `Alt + ПКМ` | Изменение размера окна |
| `Alt + Shift + ←/→` | Переключение главного монитора |

## Сборка из исходников

```bash
git clone https://github.com/lyrka-meow/Nemac.git
cd Nemac
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo cp nemac /usr/local/bin/
```

## Зависимости (Arch)

```bash
sudo pacman -S --needed xorg-server xorg-xinit libx11 libxcomposite libxdamage \
  libxfixes libxinerama libxft glew mesa imlib2 dbus qt6-base cmake
```

## Архитектура

```
src/
├── core/          # Точка входа, одновременный запуск всех компонентов
├── compositor/    # GLX-композитор (blur, тени, бар, медиа-панель)
├── wm/            # Управление окнами (floating, зоны, закреп, fullscreen)
├── monitor/       # Мультимонитор, определение главного экрана
├── gpu/           # Определение GPU (NVIDIA/AMD/гибрид), env-переменные
├── wallpaper/     # Обои (Imlib2, рандом при первом запуске)
├── updater/       # Фоновое автообновление из GitHub releases
└── mpris/         # Медиа-интеграция (D-Bus)
panel/             # Qt-панель (отдельный процесс)
tools/             # Установщик, сборка, релизы
```
