#ifndef SYNTAX_HIGHLIGHTER_H
#define SYNTAX_HIGHLIGHTER_H

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SH_MAX_LINE_LENGTH 1024

// Data structures for syntax rules and definitions
typedef struct SH_SyntaxRule
{
    char **tokens;       // Array of token strings
    int token_count;
    short color_pair;    // ncurses color pair index
    int r, g, b;         // RGB color values (0-255)
} SH_SyntaxRule;

typedef struct SH_SyntaxDefinition
{
    char **extensions;       // File extension strings (e.g. ".c", ".h")
    int ext_count;
    SH_SyntaxRule *rules;    // Array of syntax rules
    int rule_count;
} SH_SyntaxDefinition;

typedef struct SH_SyntaxDefinitions
{
    SH_SyntaxDefinition *definitions;
    int count;
} SH_SyntaxDefinitions;

// Helper: trim whitespace from both ends of a string
static inline char *sh_trim_whitespace(char *str)
{
    char *end;

    while (isspace((unsigned char)*str))
    {
        str++;
    }

    if (*str == 0)
    {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }

    return str;
}

// Parses a single rule line like:
//   "int", "double" = (255,0,0);
static inline int sh_parse_rule_line(char *line, SH_SyntaxRule *rule)
{
    // Initialize rule
    rule->tokens = NULL;
    rule->token_count = 0;
    rule->r = rule->g = rule->b = 0;

    char *p = line;
    // Extract tokens between quotes
    while ((p = strchr(p, '\"')) != NULL)
    {
        p++; // move past the first quote
        char *end_quote = strchr(p, '\"');
        if (!end_quote)
        {
            break;
        }
        int len = end_quote - p;
        char *token = (char *)malloc(len + 1);
        strncpy(token, p, len);
        token[len] = '\0';

        rule->tokens = (char **)realloc(rule->tokens, sizeof(char *) * (rule->token_count + 1));
        rule->tokens[rule->token_count] = token;
        rule->token_count++;

        p = end_quote + 1;
    }

    // Look for '=' then '(' to get the RGB values
    char *eq = strchr(line, '=');
    if (!eq)
    {
        return -1;
    }
    char *paren = strchr(eq, '(');
    if (!paren)
    {
        return -1;
    }
    int r, g, b;
    if (sscanf(paren, " ( %d , %d , %d )", &r, &g, &b) < 3 &&
        sscanf(paren, " (%d,%d,%d)", &r, &g, &b) < 3)
    {
        return -1;
    }
    rule->r = r;
    rule->g = g;
    rule->b = b;

    return 0;
}

// Loads syntax definitions from a custom syntax file (e.g., "highlight.syntax").
// The file format should be like:
//
//   SYNTAX ".h" && ".c"
//   {
//       "int", "double" = (255,0,0);
//       "for", "while" = (0,255,0);
//       "if", "else" = (0,0,255);
//   }
static inline SH_SyntaxDefinitions sh_load_syntax_definitions(const char *filename)
{
    SH_SyntaxDefinitions defs;
    defs.definitions = NULL;
    defs.count = 0;

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("fopen");
        return defs;
    }

    char line[SH_MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), fp))
    {
        char *trimmed = sh_trim_whitespace(line);
        if (strncmp(trimmed, "SYNTAX", 6) == 0)
        {
            SH_SyntaxDefinition def;
            def.extensions = NULL;
            def.ext_count = 0;
            def.rules = NULL;
            def.rule_count = 0;

            // Parse extension line: e.g. SYNTAX ".h" && ".c"
            char *p = trimmed + 6; // Skip "SYNTAX"
            while (*p)
            {
                if (*p == '\"')
                {
                    p++;
                    char *end_quote = strchr(p, '\"');
                    if (!end_quote)
                    {
                        break;
                    }
                    int len = end_quote - p;
                    char *ext = (char *)malloc(len + 1);
                    strncpy(ext, p, len);
                    ext[len] = '\0';

                    def.extensions = (char **)realloc(def.extensions, sizeof(char *) * (def.ext_count + 1));
                    def.extensions[def.ext_count] = ext;
                    def.ext_count++;

                    p = end_quote + 1;
                }
                else
                {
                    p++;
                }
            }

            // Next line should be "{"
            if (!fgets(line, sizeof(line), fp))
            {
                break;
            }
            trimmed = sh_trim_whitespace(line);
            if (trimmed[0] != '{')
            {
                continue;
            }

            // Parse rules until we hit "}"
            while (fgets(line, sizeof(line), fp))
            {
                trimmed = sh_trim_whitespace(line);
                if (trimmed[0] == '}')
                {
                    break;
                }
                if (strlen(trimmed) == 0)
                {
                    continue;
                }

                SH_SyntaxRule rule;
                if (sh_parse_rule_line(trimmed, &rule) == 0)
                {
                    def.rules = (SH_SyntaxRule *)realloc(def.rules, sizeof(SH_SyntaxRule) * (def.rule_count + 1));
                    def.rules[def.rule_count] = rule;
                    def.rule_count++;
                }
            }

            defs.definitions = (SH_SyntaxDefinition *)realloc(defs.definitions, sizeof(SH_SyntaxDefinition) * (defs.count + 1));
            defs.definitions[defs.count] = def;
            defs.count++;
        }
    }

    fclose(fp);
    return defs;
}

// Free memory allocated for syntax definitions.
static inline void sh_free_syntax_definitions(SH_SyntaxDefinitions defs)
{
    for (int i = 0; i < defs.count; i++)
    {
        SH_SyntaxDefinition def = defs.definitions[i];
        for (int j = 0; j < def.ext_count; j++)
        {
            free(def.extensions[j]);
        }
        free(def.extensions);
        for (int k = 0; k < def.rule_count; k++)
        {
            SH_SyntaxRule rule = def.rules[k];
            for (int t = 0; t < rule.token_count; t++)
            {
                free(rule.tokens[t]);
            }
            free(rule.tokens);
        }
        free(def.rules);
    }
    free(defs.definitions);
}

// Check if the given filename ends with one of the extensions in def.
static inline int sh_file_has_extension(const char *filename, SH_SyntaxDefinition def)
{
    for (int i = 0; i < def.ext_count; i++)
    {
        char *ext = def.extensions[i];
        size_t ext_len = strlen(ext);
        size_t fname_len = strlen(filename);
        if (fname_len >= ext_len)
        {
            if (strcmp(filename + fname_len - ext_len, ext) == 0)
            {
                return 1;
            }
        }
    }
    return 0;
}

// Initialize ncurses colors for each syntax rule in the definition.
// Each rule is assigned a unique color pair (starting at pair index 1 and color index 16).
static inline void sh_init_syntax_colors(SH_SyntaxDefinition *def)
{
    short next_color_index = 16;
    short next_pair_index = 1; // pair 0 is default

    for (int i = 0; i < def->rule_count; i++)
    {
        SH_SyntaxRule *rule = &def->rules[i];
        short color_number = next_color_index++;
        short pair_index = next_pair_index++;

        // Scale 0-255 values to 0-1000 for ncurses
        short r_scaled = (rule->r * 1000) / 255;
        short g_scaled = (rule->g * 1000) / 255;
        short b_scaled = (rule->b * 1000) / 255;
        if (can_change_color())
        {
            init_color(color_number, r_scaled, g_scaled, b_scaled);
        }
        init_pair(pair_index, color_number, -1);
        rule->color_pair = pair_index;
    }
}

// Highlights the given file in the provided ncurses window using the syntax definition.
// This function reads the file line by line and prints tokens using their defined colors.
static inline void sh_highlight_file(WINDOW *win, const char *filename, SH_SyntaxDefinition *def)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        wprintw(win, "Error opening file: %s\n", filename);
        return;
    }

    char buffer[SH_MAX_LINE_LENGTH];
    int row = 0;
    while (fgets(buffer, sizeof(buffer), fp))
    {
        buffer[strcspn(buffer, "\n")] = '\0';
        int len = strlen(buffer);
        int col = 0;
        int i = 0;
        while (i < len)
        {
            if (isalpha(buffer[i]) || buffer[i] == '_')
            {
                int start = i;
                while (i < len && (isalnum(buffer[i]) || buffer[i] == '_'))
                {
                    i++;
                }
                int word_len = i - start;
                char *word = (char *)malloc(word_len + 1);
                strncpy(word, buffer + start, word_len);
                word[word_len] = '\0';

                int highlighted = 0;
                for (int r = 0; r < def->rule_count; r++)
                {
                    SH_SyntaxRule rule = def->rules[r];
                    for (int t = 0; t < rule.token_count; t++)
                    {
                        if (strcmp(word, rule.tokens[t]) == 0)
                        {
                            wattron(win, COLOR_PAIR(rule.color_pair));
                            mvwprintw(win, row, col, "%s", word);
                            wattroff(win, COLOR_PAIR(rule.color_pair));
                            highlighted = 1;
                            break;
                        }
                    }
                    if (highlighted)
                    {
                        break;
                    }
                }
                if (!highlighted)
                {
                    mvwprintw(win, row, col, "%s", word);
                }
                col += word_len;
                free(word);
            }
            else
            {
                mvwaddch(win, row, col, buffer[i]);
                i++;
                col++;
            }
        }
        row++;
    }

    fclose(fp);
    wrefresh(win);
}

#ifdef __cplusplus
}
#endif

#endif // SYNTAX_HIGHLIGHTER_H
