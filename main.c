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
        // Use sscanf to parse up to the '=' and then the value up to the ';'
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
// Refreshes the screen using erase() and batching updates with wnoutrefresh/doupdate
// to reduce flickering.
//
void editor_refresh_screen(void)
{
    erase(); // Clears the virtual window without a full terminal clear.
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    update_viewport();

    // Display the text buffer with line numbers.
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

    // Display a status/help line at the bottom.
    mvprintw(rows - 1, 0, "Ctrl+Q: Quit | Ctrl+S: Save | Ctrl+O: Open | Tab: %s | Arrow keys: Move",
             config.tab_four_spaces ? "4 spaces" : "tab");

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
    // Shift characters to the right from the cursor.
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
// Inserts a new line. If AUTO_INDENT is enabled, the new line is prefilled
// with the same leading spaces as the current line.
//
void editor_insert_newline(void)
{
    if (editor.num_lines >= MAX_LINES)
    {
        return;
    }
    
    char *line = editor.text[editor.cursor_y];
    int len = strlen(line);
    
    // Save remainder from the current line.
    char remainder[MAX_COLS];
    strcpy(remainder, line + editor.cursor_x);
    
    // Terminate current line at cursor.
    line[editor.cursor_x] = '\0';
    
    // Shift lines below down by one.
    for (int i = editor.num_lines; i > editor.cursor_y + 1; i--)
    {
        strcpy(editor.text[i], editor.text[i - 1]);
    }
    
    if (config.auto_indent)
    {
        // Count leading spaces (indentation) of current line.
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
// Saves the file. If a file is already associated with this session,
// it saves directly; otherwise, it prompts for a filename and sets the save state.
//
void editor_save_file(void)
{
    char filename[PROMPT_BUFFER_SIZE];
    char filepath[PROMPT_BUFFER_SIZE];

    // If a file is already associated, save directly.
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
        // Set the save state.
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

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    mvprintw(rows - 1, 0, "File saved as %s. Press any key...", filepath);
    clrtoeol();
    getch();
}

//
// Loads a file by prompting for its name and reading from the "saves" directory.
// When a file is loaded, the save state is updated so subsequent saves write directly.
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
    // Update the save state.
    strncpy(current_file, filepath, PROMPT_BUFFER_SIZE);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    mvprintw(rows - 1, 0, "File loaded from %s. Press any key...", filepath);
    clrtoeol();
    getch();
}

void process_keypress(void)
{
    int ch = getch();
    switch (ch)
    {
        case 17: // Ctrl+Q to quit.
        {
            endwin();
            exit(0);
            break;
        }
        case 19: // Ctrl+S to save.
        {
            editor_save_file();
            break;
        }
        case 15: // Ctrl+O to open.
        {
            editor_load_file();
            break;
        }
        case '\t': // Tab key.
        {
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
        }
        case KEY_LEFT:
        {
            if (editor.cursor_x > 0)
            {
                editor.cursor_x--;
            }
            else if (editor.cursor_y > 0)
            {
                editor.cursor_y--;
                editor.cursor_x = strlen(editor.text[editor.cursor_y]);
            }
            break;
        }
        case KEY_RIGHT:
        {
            if (editor.cursor_x < (int)strlen(editor.text[editor.cursor_y]))
            {
                editor.cursor_x++;
            }
            else if (editor.cursor_y < editor.num_lines - 1)
            {
                editor.cursor_y++;
                editor.cursor_x = 0;
            }
            break;
        }
        case KEY_UP:
        {
            if (editor.cursor_y > 0)
            {
                editor.cursor_y--;
                int line_len = strlen(editor.text[editor.cursor_y]);
                if (editor.cursor_x > line_len)
                {
                    editor.cursor_x = line_len;
                }
            }
            break;
        }
        case KEY_DOWN:
        {
            if (editor.cursor_y < editor.num_lines - 1)
            {
                editor.cursor_y++;
                int line_len = strlen(editor.text[editor.cursor_y]);
                if (editor.cursor_x > line_len)
                {
                    editor.cursor_x = line_len;
                }
            }
            break;
        }
        case KEY_BACKSPACE:
        case 127:
        {
            editor_delete_char();
            break;
        }
        case KEY_DC:
        {
            editor_delete_at_cursor();
            break;
        }
        case '\n':
        case '\r':
        {
            editor_insert_newline();
            break;
        }
        default:
        {
            if (ch >= 32 && ch <= 126)
            {
                editor_insert_char(ch);
            }
            break;
        }
    }
}

int main(void)
{
    // Load settings from file.
    load_config();

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    // Show a block-style cursor.
    curs_set(1);

    init_editor();

    while (1)
    {
        editor_refresh_screen();
        process_keypress();
    }

    endwin();
    return 0;
}
