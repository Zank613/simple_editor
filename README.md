# Simple editor
Custom terminal-based text editor written in C99 and ncurses library. Yes it is single file.

## Features
- CTRL+S to save. Save files are created by making a saves directory in the root directory of the program.
- CTRL+O to load a file.
- Tab is 4 spaces.
- Auto indentation.
- Line numbers on the left side.
- Tweaking settings with settings.config file.
- CTRL+Y for Redo & CTRL+Z for Undo.
- Mouse wheel to control cursor.
- Status bar on the bottom.
- Home/End keys jump to start or end of the line.
- Page up/down scroll by 5 lines.

## Prerequisites
- Use Linux.
- Have ncurses installed.
- Have GCC or any other C compiler installed.

## How to use
### Clone Git repo
```bash
git clone https://github.com/Zank613/simple_editor.git
```

### Build it
```bash
gcc -o custom_editor main.c -lncurses
```
### Run it
```bash
./custom_editor
```

### Tweak the settings.config (Optional)
```c
TAB_FOUR_SPACES = TRUE;
AUTO_INDENT = TRUE;
```
## Acknowledgements
- **[ncurses](https://invisible-island.net/ncurses/ncurses.html)**

## [LICENSE](https://github.com/Zank613/simple_editor/blob/master/LICENSE)