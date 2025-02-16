# Simple editor **Experimental**
Custom terminal-based text editor written in C99 and ncurses library. Experimental branch supporting syntax highlighting.

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
- **Syntax highlighting.**

    - **Support for basic C99 keywords.**
    - **CTRL+H to toggle syntax highlighting on or off.**

## **Experimental Branch**
- **Syntax highlighting extension added to main codebase.**

- **highlight.syntax file to parse and add color to the keywords.**

## Prerequisites
- Use Linux.

- Have ncurses installed.
- Have GCC or any other C compiler installed.

## How to use
### Clone Git repo
```bash
git clone -b syntax_highlight_experimental --single-branch https://github.com/Zank613/simple_editor.git
```

### Build it
```bash
gcc -o custom_editor_exp main.c -lncurses
```
### Run it
```bash
./custom_editor_exp
```

### Tweak the settings.config (Optional)
```c
TAB_FOUR_SPACES = TRUE;
AUTO_INDENT = TRUE;
```

### **Added new syntax features to highlight.syntax**
```text
SYNTAX ".c" && ".h"
{
    "char", "short", "int", "long", "float", "double", "void", "_Bool", "_Complex", "_Imaginary" = (255, 0, 0);
    "if", "else", "switch", "case", "default", "for", "while", "do", "break", "continue", "return", "goto" = (0, 255, 0);
    "auto", "extern", "static", "register", "const", "volatile", "restrict", "inline" = (0, 0, 255);
    "struct", "union", "enum", "typedef" = (255, 255, 0);
}
```
## Acknowledgements
- **[ncurses](https://invisible-island.net/ncurses/ncurses.html)**

## [LICENSE](https://github.com/Zank613/simple_editor/blob/master/LICENSE)