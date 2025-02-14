#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_LINES 1000
#define MAX_COLS 1024
#define LINE_NUMBER_WIDTH 6
#define PROMPT_BUFFER_SIZE 256
#define UNDO_STACK_SIZE 100

// Configuration structure.
typedef struct Config
{
    int tab_four_spaces;
    int auto_indent;
    // Add more configuration options here.
} Config;

// Default configuration: both enabled.
Config config = { 1, 1 };

// Global variable to store current file path (save state).
char current_file[PROMPT_BUFFER_SIZE] = { 0 };

// Dirty flag: 1 if file has unsaved changes.
int dirty = 0;

// Helper: trim leading and trailing whitespace.
char *trim(char *s)
{
    while (isspace((unsigned char)*s))
    {
        s++;
    }
    if (*s == '\0')
    {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
    {
        end--;
    }
    *(end + 1) = '\0';
    return s;
}

//
// Loads the configuration from "settings.config".
// The file should have lines like:
//   TAB_FOUR_SPACES = TRUE;
//   AUTO_INDENT = TRUE;
//
void load_config(void)
{
    FILE *fp = fopen("settings.config", "r");
    if (fp == NULL)
    {
        // No config file found; use default settings.
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        // Skip leading whitespace.
        char *p = line;
        while (*p && isspace((unsigned char)*p))
        {
            p++;
        }
        // Skip blank lines or comments.
        if (*p == '\0' || *p == '\n' || *p == '/' || *p == '#')
        {
            continue;
        }

        char key[64], value[64];
        // Use sscanf to parse key=value; lines.
        if (sscanf(p, " %63[^=] = %63[^;];", key, value) == 2)
        {
            char *trimmedKey = trim(key);
            char *trimmedValue = trim(value);
            if (strcmp(trimmedKey, "TAB_FOUR_SPACES") == 0)
            {
                config.tab_four_spaces = (strcmp(trimmedValue, "TRUE") == 0 || strcmp(trimmedValue, "true") == 0);
            }
            else if (strcmp(trimmedKey, "AUTO_INDENT") == 0)
            {
                config.auto_indent = (strcmp(trimmedValue, "TRUE") == 0 || strcmp(trimmedValue, "true") == 0);
            }
            // Add more options here if needed.
        }
    }
    fclose(fp);
}

typedef struct Editor
{
    char text[MAX_LINES][MAX_COLS];
    int num_lines;
    int cursor_x;
    int cursor_y;
    int row_offset;
    int col_offset;
} Editor;

Editor editor;

void init_editor(void)
{
    memset(editor.text, 0, sizeof(editor.text));
    editor.num_lines = 1;
    editor.cursor_x = 0;
    editor.cursor_y = 0;
    editor.row_offset = 0;
    editor.col_offset = 0;
}

void update_viewport(void)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Vertical scrolling:
    if (editor.cursor_y < editor.row_offset)
    {
        editor.row_offset = editor.cursor_y;
    }
    else if (editor.cursor_y >= editor.row_offset + (rows - 1))
    {
        editor.row_offset = editor.cursor_y - (rows - 2);
    }

    // Horizontal scrolling:
    int usable_cols = cols - LINE_NUMBER_WIDTH;
    if (editor.cursor_x < editor.col_offset)
    {
        editor.col_offset = editor.cursor_x;
    }
    else if (editor.cursor_x >= editor.col_offset + usable_cols)
    {
        editor.col_offset = editor.cursor_x - usable_cols + 1;
    }
}

//
// Undo/Redo: store snapshots of the editor state.
//
typedef struct EditorState
{
    char text[MAX_LINES][MAX_COLS];
    int num_lines;
    int cursor_x;
    int cursor_y;
    int row_offset;
    int col_offset;
} EditorState;

EditorState undo_stack[UNDO_STACK_SIZE];
int undo_stack_top = 0;

EditorState redo_stack[UNDO_STACK_SIZE];
int redo_stack_top = 0;

//
// Save the current state into the undo stack and clear the redo stack.
//
void save_state_undo(void)
{
    if (undo_stack_top < UNDO_STACK_SIZE)
    {
        EditorState state;
        memcpy(state.text, editor.text, sizeof(editor.text));
        state.num_lines = editor.num_lines;
        state.cursor_x = editor.cursor_x;
        state.cursor_y = editor.cursor_y;
        state.row_offset = editor.row_offset;
        state.col_offset = editor.col_offset;
        undo_stack[undo_stack_top++] = state;
    }
    // Clear redo stack:
    redo_stack_top = 0;
    dirty = 1;
}

//
// Undo: pop a state from the undo stack and push the current state onto the redo stack.
//
void undo(void)
{
    if (undo_stack_top > 0)
    {
        EditorState state = undo_stack[--undo_stack_top];
        if (redo_stack_top < UNDO_STACK_SIZE)
        {
            EditorState current;
            memcpy(current.text, editor.text, sizeof(editor.text));
            current.num_lines = editor.num_lines;
            current.cursor_x = editor.cursor_x;
            current.cursor_y = editor.cursor_y;
            current.row_offset = editor.row_offset;
            current.col_offset = editor.col_offset;
            redo_stack[redo_stack_top++] = current;
        }
        memcpy(editor.text, state.text, sizeof(editor.text));
        editor.num_lines = state.num_lines;
        editor.cursor_x = state.cursor_x;
        editor.cursor_y = state.cursor_y;
        editor.row_offset = state.row_offset;
        editor.col_offset = state.col_offset;
        dirty = 1;
    }
}

//
// Redo: pop a state from the redo stack and push the current state onto the undo stack.
//
void redo(void)
{
    if (redo_stack_top > 0)
    {
        EditorState state = redo_stack[--redo_stack_top];
        if (undo_stack_top < UNDO_STACK_SIZE)
        {
            EditorState current;
            memcpy(current.text, editor.text, sizeof(editor.text));
            current.num_lines = editor.num_lines;
            current.cursor_x = editor.cursor_x;
            current.cursor_y = editor.cursor_y;
            current.row_offset = editor.row_offset;
            current.col_offset = editor.col_offset;
            undo_stack[undo_stack_top++] = current;
        }
        memcpy(editor.text, state.text, sizeof(editor.text));
        editor.num_lines = state.num_lines;
        editor.cursor_x = state.cursor_x;
        editor.cursor_y = state.cursor_y;
        editor.row_offset = state.row_offset;
        editor.col_offset = state.col_offset;
        dirty = 1;
    }
}

//
// Refreshes the screen using erase() and batching updates with wnoutrefresh/doupdate
// to reduce flickering. Also updates a detailed status bar.
//
void editor_refresh_screen(void)
{
    erase(); // Clear virtual window.

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    update_viewport();

    // Display text with line numbers.
    for (int i = editor.row_offset; i < editor.num_lines && i < editor.row_offset + rows - 1; i++)
    {
        int screen_row = i - editor.row_offset;
        mvprintw(screen_row, 0, "%4d |", i + 1);

        char *line = editor.text[i];
        int line_len = strlen(line);
        int usable_cols = cols - LINE_NUMBER_WIDTH;
        if (editor.col_offset < line_len)
        {
            mvprintw(screen_row, LINE_NUMBER_WIDTH, "%.*s", usable_cols, line + editor.col_offset);
        }
    }

    // Build a status string.
    char status[256];
    const char *fname = (current_file[0] != '\0') ? current_file : "Untitled";
    snprintf(status, sizeof(status), "File: %s | Ln: %d, Col: %d%s",
             fname,
             editor.cursor_y + 1,
             editor.cursor_x + 1,
             (dirty ? " [Modified]" : ""));
    // Display the status along with help keys.
    mvprintw(rows - 1, 0, "%s  (Ctrl+Q: Quit, Ctrl+S: Save, Ctrl+O: Open, Ctrl+Z: Undo, Ctrl+Y: Redo, Home/End, PgUp/PgDn, Mouse)", status);

    int screen_cursor_y = editor.cursor_y - editor.row_offset;
    int screen_cursor_x = editor.cursor_x - editor.col_offset + LINE_NUMBER_WIDTH;
    move(screen_cursor_y, screen_cursor_x);

    wnoutrefresh(stdscr);
    doupdate();
}

void editor_insert_char(int ch)
{
    char *line = editor.text[editor.cursor_y];
    int len = strlen(line);
    if (len >= MAX_COLS - 1)
    {
        return;
    }
    // Shift characters to the right.
    for (int i = len; i >= editor.cursor_x; i--)
    {
        line[i + 1] = line[i];
    }
    line[editor.cursor_x] = ch;
    editor.cursor_x++;
}

void editor_delete_char(void)
{
    if (editor.cursor_x == 0)
    {
        if (editor.cursor_y == 0)
        {
            return;
        }
        int prev_len = strlen(editor.text[editor.cursor_y - 1]);
        int curr_len = strlen(editor.text[editor.cursor_y]);
        if (prev_len + curr_len >= MAX_COLS - 1)
        {
            return;
        }
        strcat(editor.text[editor.cursor_y - 1], editor.text[editor.cursor_y]);
        for (int i = editor.cursor_y; i < editor.num_lines - 1; i++)
        {
            strcpy(editor.text[i], editor.text[i + 1]);
        }
        editor.num_lines--;
        editor.cursor_y--;
        editor.cursor_x = prev_len;
    }
    else
    {
        char *line = editor.text[editor.cursor_y];
        int len = strlen(line);
        for (int i = editor.cursor_x - 1; i < len; i++)
        {
            line[i] = line[i + 1];
        }
        editor.cursor_x--;
    }
}

void editor_delete_at_cursor(void)
{
    char *line = editor.text[editor.cursor_y];
    int len = strlen(line);
    if (editor.cursor_x == len)
    {
        if (editor.cursor_y == editor.num_lines - 1)
        {
            return;
        }
        int next_len = strlen(editor.text[editor.cursor_y + 1]);
        if (len + next_len >= MAX_COLS - 1)
        {
            return;
        }
        strcat(line, editor.text[editor.cursor_y + 1]);
        for (int i = editor.cursor_y + 1; i < editor.num_lines - 1; i++)
        {
            strcpy(editor.text[i], editor.text[i + 1]);
        }
        editor.num_lines--;
    }
    else
    {
        for (int i = editor.cursor_x; i < len; i++)
        {
            line[i] = line[i + 1];
        }
    }
}

//
// Inserts a new line. If AUTO_INDENT is enabled, prefill with same leading spaces.
//
void editor_insert_newline(void)
{
    if (editor.num_lines >= MAX_LINES)
    {
        return;
    }
    
    char *line = editor.text[editor.cursor_y];
    int len = strlen(line);
    
    char remainder[MAX_COLS];
    strcpy(remainder, line + editor.cursor_x);
    
    line[editor.cursor_x] = '\0';
    
    // Shift lines down.
    for (int i = editor.num_lines; i > editor.cursor_y + 1; i--)
    {
        strcpy(editor.text[i], editor.text[i - 1]);
    }
    
    if (config.auto_indent)
    {
        int indent_count = 0;
        while (line[indent_count] == ' ' && indent_count < MAX_COLS - 1)
        {
            indent_count++;
        }
        char new_line[MAX_COLS] = {0};
        for (int i = 0; i < indent_count && i < MAX_COLS - 1; i++)
        {
            new_line[i] = ' ';
        }
        new_line[indent_count] = '\0';
        strncat(new_line, remainder, MAX_COLS - strlen(new_line) - 1);
        strcpy(editor.text[editor.cursor_y + 1], new_line);
        editor.cursor_x = indent_count;
    }
    else
    {
        strcpy(editor.text[editor.cursor_y + 1], remainder);
        editor.cursor_x = 0;
    }
    editor.num_lines++;
    editor.cursor_y++;
}

//
// Prompts for input at the bottom of the screen.
//
void editor_prompt(char *prompt, char *buffer, size_t bufsize)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    move(rows - 1, 0);
    clrtoeol();
    mvprintw(rows - 1, 0, "%s", prompt);
    echo();
    curs_set(1);
    getnstr(buffer, bufsize - 1);
    noecho();
    curs_set(1);
}

//
// Saves the file. If already associated, writes directly;
// otherwise, prompts for a filename and sets save state.
//
void editor_save_file(void)
{
    char filename[PROMPT_BUFFER_SIZE];
    char filepath[PROMPT_BUFFER_SIZE];

    if (current_file[0] != '\0')
    {
        strncpy(filepath, current_file, PROMPT_BUFFER_SIZE);
    }
    else
    {
        editor_prompt("Save as: ", filename, PROMPT_BUFFER_SIZE);
        if (strlen(filename) == 0)
        {
            return;
        }
        struct stat st = {0};
        if (stat("saves", &st) == -1)
        {
            if (mkdir("saves", 0777) == -1)
            {
                int rows, cols;
                getmaxyx(stdscr, rows, cols);
                mvprintw(rows - 1, 0, "Error creating saves directory: %s", strerror(errno));
                getch();
                return;
            }
        }
        snprintf(filepath, PROMPT_BUFFER_SIZE, "saves/%s", filename);
        strncpy(current_file, filepath, PROMPT_BUFFER_SIZE);
    }

    FILE *fp = fopen(filepath, "w");
    if (fp == NULL)
    {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        mvprintw(rows - 1, 0, "Error opening file for writing: %s", strerror(errno));
        getch();
        return;
    }

    for (int i = 0; i < editor.num_lines; i++)
    {
        fprintf(fp, "%s\n", editor.text[i]);
    }
    fclose(fp);
    dirty = 0; // Reset dirty flag on save.

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    mvprintw(rows - 1, 0, "File saved as %s. Press any key...", filepath);
    clrtoeol();
    getch();
}

//
// Loads a file from the "saves" directory and updates save state.
//
void editor_load_file(void)
{
    char filename[PROMPT_BUFFER_SIZE];
    char filepath[PROMPT_BUFFER_SIZE];

    editor_prompt("Open file: ", filename, PROMPT_BUFFER_SIZE);
    if (strlen(filename) == 0)
    {
        return;
    }
    snprintf(filepath, PROMPT_BUFFER_SIZE, "saves/%s", filename);
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL)
    {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        mvprintw(rows - 1, 0, "Error opening file for reading: %s", strerror(errno));
        getch();
        return;
    }

    memset(editor.text, 0, sizeof(editor.text));
    editor.num_lines = 0;

    char line_buffer[MAX_COLS];
    while (fgets(line_buffer, MAX_COLS, fp) && editor.num_lines < MAX_LINES)
    {
        size_t len = strlen(line_buffer);
        if (len > 0 && line_buffer[len - 1] == '\n')
        {
            line_buffer[len - 1] = '\0';
        }
        strncpy(editor.text[editor.num_lines], line_buffer, MAX_COLS - 1);
        editor.num_lines++;
    }
    fclose(fp);

    editor.cursor_x = 0;
    editor.cursor_y = 0;
    strncpy(current_file, filepath, PROMPT_BUFFER_SIZE);
    dirty = 0;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    mvprintw(rows - 1, 0, "File loaded from %s. Press any key...", filepath);
    clrtoeol();
    getch();
}

//
// Processes key presses, including simple mouse support (click to move,
// mouse wheel scrolling), Home/End, and Page Up/Page Down.
//
void process_keypress(void)
{
    int ch = getch();
    if (ch == KEY_MOUSE)
    {
        MEVENT event;
        if (getmouse(&event) == OK)
        {
            if (event.bstate & BUTTON1_CLICKED)
            {
                // Move cursor to clicked position.
                int new_cursor_y = event.y + editor.row_offset;
                int new_cursor_x;
                if (event.x < LINE_NUMBER_WIDTH)
                    new_cursor_x = 0;
                else
                    new_cursor_x = event.x - LINE_NUMBER_WIDTH + editor.col_offset;
                if (new_cursor_y >= editor.num_lines)
                    new_cursor_y = editor.num_lines - 1;
                int line_len = strlen(editor.text[new_cursor_y]);
                if (new_cursor_x > line_len)
                    new_cursor_x = line_len;
                editor.cursor_y = new_cursor_y;
                editor.cursor_x = new_cursor_x;
            }
            else if (event.bstate & BUTTON4_PRESSED) // Mouse wheel up.
            {
                editor.cursor_y -= 3;
                if (editor.cursor_y < 0)
                    editor.cursor_y = 0;
            }
            else if (event.bstate & BUTTON5_PRESSED) // Mouse wheel down.
            {
                editor.cursor_y += 3;
                if (editor.cursor_y >= editor.num_lines)
                    editor.cursor_y = editor.num_lines - 1;
            }
        }
        return;
    }

    switch (ch)
    {
        case 17: // Ctrl+Q: Quit.
            endwin();
            exit(0);
            break;
        case 26: // Ctrl+Z: Undo.
            undo();
            break;
        case 25: // Ctrl+Y: Redo.
            redo();
            break;
        case 19: // Ctrl+S: Save.
            editor_save_file();
            break;
        case 15: // Ctrl+O: Open.
            editor_load_file();
            break;
        case KEY_HOME:
            editor.cursor_x = 0;
            break;
        case KEY_END:
        {
            int len = strlen(editor.text[editor.cursor_y]);
            editor.cursor_x = len;
            break;
        }
        case KEY_PPAGE: // Page Up.
            editor.cursor_y -= 5;
            if (editor.cursor_y < 0)
                editor.cursor_y = 0;
            break;
        case KEY_NPAGE: // Page Down.
            editor.cursor_y += 5;
            if (editor.cursor_y >= editor.num_lines)
                editor.cursor_y = editor.num_lines - 1;
            break;
        case '\t': // Tab key.
            save_state_undo();
            if (config.tab_four_spaces)
            {
                for (int i = 0; i < 4; i++)
                {
                    editor_insert_char(' ');
                }
            }
            else
            {
                editor_insert_char('\t');
            }
            break;
        case KEY_LEFT:
            if (editor.cursor_x > 0)
                editor.cursor_x--;
            else if (editor.cursor_y > 0)
            {
                editor.cursor_y--;
                editor.cursor_x = strlen(editor.text[editor.cursor_y]);
            }
            break;
        case KEY_RIGHT:
            if (editor.cursor_x < (int)strlen(editor.text[editor.cursor_y]))
                editor.cursor_x++;
            else if (editor.cursor_y < editor.num_lines - 1)
            {
                editor.cursor_y++;
                editor.cursor_x = 0;
            }
            break;
        case KEY_UP:
            if (editor.cursor_y > 0)
            {
                editor.cursor_y--;
                int line_len = strlen(editor.text[editor.cursor_y]);
                if (editor.cursor_x > line_len)
                    editor.cursor_x = line_len;
            }
            break;
        case KEY_DOWN:
            if (editor.cursor_y < editor.num_lines - 1)
            {
                editor.cursor_y++;
                int line_len = strlen(editor.text[editor.cursor_y]);
                if (editor.cursor_x > line_len)
                    editor.cursor_x = line_len;
            }
            break;
        case KEY_BACKSPACE:
        case 127:
            save_state_undo();
            editor_delete_char();
            break;
        case KEY_DC:
            save_state_undo();
            editor_delete_at_cursor();
            break;
        case '\n':
        case '\r':
            save_state_undo();
            editor_insert_newline();
            break;
        default:
            if (ch >= 32 && ch <= 126)
            {
                save_state_undo();
                editor_insert_char(ch);
            }
            break;
    }
}

int main(void)
{
    // Load settings.
    load_config();

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1); // Block-style cursor.
    // Enable mouse events.
    mousemask(ALL_MOUSE_EVENTS, NULL);
    mouseinterval(0);

    init_editor();

    while (1)
    {
        editor_refresh_screen();
        process_keypress();
    }

    endwin();
    return 0;
}
