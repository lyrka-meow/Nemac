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
  libxfixes libxinerama libxft glew mesa imlib2 dbus qt6-base qt6-declarative cmake
```

## Архитектура

```
src/
├── core/
├── compositor/
├── wm/
├── monitor/
├── gpu/
├── wallpaper/
├── updater/
└── mpris/
panel/
tools/
```

- **core** — точка входа, запуск компонентов
- **compositor** — GLX-композитор, blur, тени, бар, медиа-панель
- **wm** — floating WM, зоны, закреп, fullscreen
- **monitor** — мультимонитор
- **gpu** — определение GPU, env-переменные
- **wallpaper** — обои через Imlib2
- **updater** — автообновление из GitHub
- **mpris** — медиа-интеграция через D-Bus
- **panel** — Qt-панель (отдельный процесс)
- **tools** — установщик, сборка
