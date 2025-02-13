# Simple editor
Custom editor written in C99 and ncurses library. Yes it is single file.

## Features
- CTRL+S to save. Save files are created by making a saves directory in the root directory of the program.
- CTRL+O to load a file.
- Tab is 4 spaces.
- Auto indentation.
- Line numbers on the left side.
- Tweaking settings with settings.config file.

## Prerequisites
- Use Linux.
- Have ncurses installed.
- Have GCC installed.

## How to use
### Clone Git repo
```bash
git clone https://github.com/Zank613/simple_editor.git
```

### Build it
```bash
gcc -o custom_editor main.c -lncurses
```

### Tweak the settings.config
```c
TAB_FOUR_SPACES = TRUE;
AUTO_INDENT = TRUE;
```