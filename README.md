# zenfetch

A minimal system information display tool featuring an animated bonsai tree.

zenfetch combines the beauty of [cbonsai](https://gitlab.com/jallbrit/cbonsai)
with system information display, similar to neofetch but with a zen twist.

## Features

- Animated bonsai tree growth on each run
- System information: OS, uptime, CPU, memory, storage, network, IP, local time
- Configurable owner, location, support contact, and documentation URL
- Noir mode for monochrome terminals
- Clickable hyperlinks for URLs and emails (OSC 8 compatible terminals)
- Configuration via CLI flags or `/etc/zenfetch/` config files

## Screenshot

```
                          -=:. ... -=:.: ...
                            -=:.——=:. ...
                          ——=:.. :. ——=:....
                    -=:.  -=:.-=:.-=:-=:.. :.:..
                  -=:... -=:.. :.-=:.-=-:.:.:..:.
                    -=:-=:.. =:.#——-=-=:.-=:.
                        ## #-=:. %: -=:-=:.
                        #  -=:.. :.%:..
                      -=:.-#-=:.:..  -=%.
                    -=:..%=:.*%#     %#%
                      -=:.%-=:. % %#  #%
                    -=:....%#%%#  %#%
                      -=:.-=:.  %#%  %#**
                    -=:.-=:...*%#%##%**  -=:.-=:.              -=:.
                    -=:.. -=:..*  %*%###%  %  -=:.-=:.       -=:.
                    -=:.-=:.-=:.=:.-=:.  ####+*   -=:. ... -=:-=:.
                      -=:.            ###%*%+--=:..
                        -=:.          ###%  +* -=:.. :..
                                      ###       -=:.
                                      ###
                                      ###
                                     #####
                                    *#####*
                  . :: —— == ++ ****###########****++ == —— :: .

OS                 Ubuntu 22.04.3 LTS (5.15.0-91-generic)
UPTIME             14d 3h 22m
HARDWARE           Intel(R) Core(TM) i7-10700 CPU 8 core
MEMORY             4521 MB / 32000 MB
STORAGE            142.3G / 500.0G
NETWORK BANDWIDTH  1000 Mbps (Ethernet)
NODE IP            192.168.1.100
LOCATION           Data Center 1
LOCAL TIME         February 03 2026, 10:30:45 AM EST

SUPPORT            support@example.com
DOCS               docs.example.com
```

## Installation

### Dependencies

zenfetch requires ncurses. Install the development libraries for your system:

**Debian/Ubuntu:**

```bash
sudo apt install libncursesw5-dev
```

**Fedora:**

```bash
sudo dnf install ncurses-devel
```

**macOS:**

```bash
brew install ncurses
```

### Build & Install

```bash
git clone https://github.com/punctuations/zenfetch
cd zenfetch
make
```

This installs both `zenfetch` and `cbonsai` to `/usr/local/bin`.

To install for the current user only:

```bash
make install PREFIX=~/.local
```

## Usage

```
Usage: zenfetch [OPTIONS]

Display system info with a bonsai tree.

Options:
  -o, --owner TEXT      set owner name in welcome message
  -l, --location TEXT   set location
  -s, --support TEXT    set support contact info
  -d, --docs URL        set documentation URL
  -S, --no-support      hide support/docs section
  -I, --hide-ip         hide NODE IP field
  -n, --noir            noir mode: no colors, bold labels
  -h, --help            show this help
```

### Examples

Basic usage:

```bash
zenfetch
```

With custom owner and location:

```bash
zenfetch --owner "John Doe" --location "NYC Office"
```

Server MOTD with support info:

```bash
zenfetch -o "IT Department" -s "help@company.com" -d "wiki.company.com"
```

Noir mode (no colors):

```bash
zenfetch --noir
```

Hide sensitive info:

```bash
zenfetch --hide-ip --no-support
```

### Configuration Files

For persistent configuration, create files in `/etc/zenfetch/`:

```bash
sudo mkdir -p /etc/zenfetch
echo "Your Name" | sudo tee /etc/zenfetch/owner
echo "Building A, Room 101" | sudo tee /etc/zenfetch/location
echo "support@example.com" | sudo tee /etc/zenfetch/support
echo "docs.example.com" | sudo tee /etc/zenfetch/docs
```

CLI options override config file values.

### Add to Shell Profile

Display zenfetch on every terminal login by adding to `~/.bashrc` or `~/.zshrc`:

```bash
zenfetch
```

## cbonsai

The standalone `cbonsai` tree generator is also included. See `cbonsai --help`
for options including:

- `-l, --live` - Animated growth mode
- `-i, --infinite` - Continuously generate trees
- `-S, --screensaver` - Screensaver mode
- `-m, --message` - Attach a message to the tree
- `-n, --noir` - Monochrome mode

## Credits

- [cbonsai](https://gitlab.com/jallbrit/cbonsai) by jallbrit - The original
  bonsai tree generator

## License

GPL-3.0
