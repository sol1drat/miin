#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    ND_INT,
    ND_ADD,
    ND_SUB,
    ND_OPA,
    ND_CPA,
    ND_EOF
} NodeType;

typedef struct {
    NodeType type;
    int int_value;
    size_t line_idx;
} Node;

typedef enum {
    TK_INT,
    TK_ADD,
    TK_SUB,
    TK_OPA,
    TK_CPA,
    TK_EOF
} TokenType ;

typedef struct {
    TokenType type;
    int int_value;
    size_t line_idx;
} Token;

bool isopr(char c) {
    return c == '+' || c == '-';
}

bool ispar(char c) {
    return c == '(' || c == ')';
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("no input file\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        perror("error opening file");
        return 1;
    }

    fseek(fp, 0, SEEK_END);

    long size = ftell(fp);
    if (size == -1) {
        perror("error getting file size");
        fclose(fp);
        return 1;
    }
    size_t file_size = (size_t)size;

    rewind(fp);

    char *src_buffer = malloc(file_size + 1);
    if (src_buffer == NULL) {
        perror("error allocating memory");
        fclose(fp);
        return 1;
    }

    size_t bytes_read = fread(src_buffer, 1, file_size, fp);
    if (bytes_read != file_size) {
        fprintf(stderr, "error reading file\n");
        free(src_buffer);
        fclose(fp);
        return 1;
    }

    src_buffer[file_size] = '\0';
    fclose(fp);

    Token tkn_array[64];
    size_t tkn_arr_idx = 0;
    size_t src_idx = 0;
    size_t line_idx = 1;
    size_t line_int_idx = 1;
    size_t line_end_idx = 1;

    int int_value = 0;
    bool reading_int = false;

    while (src_buffer[src_idx] != '\0') {
        char c = src_buffer[src_idx];

        if (!isdigit((unsigned char)c) && reading_int) {
            Token tkn = {
                .type = TK_INT,
                .int_value = int_value,
                .line_idx = line_int_idx,
            };

            tkn_array[tkn_arr_idx++] = tkn;

            int_value = 0;
            reading_int = false;
        }

        if (isdigit((unsigned char)c)) {
            if (!reading_int) line_int_idx = line_idx;
            int_value = int_value * 10 + (c - '0');
            reading_int = true;
        } else if (isopr(c)) {
            switch (c) {
                case '+':
                    tkn_array[tkn_arr_idx++] = (Token){ .type = TK_ADD, .line_idx = line_idx };
                    break;
                case '-':
                    tkn_array[tkn_arr_idx++] = (Token){ .type = TK_SUB, .line_idx = line_idx };
                    break;
            }
        } else if (ispar(c)) {
            switch (c) {
                case '(':
                    tkn_array[tkn_arr_idx++] = (Token){ .type = TK_OPA, .line_idx = line_idx };
                    break;
                case ')':
                    tkn_array[tkn_arr_idx++] = (Token){ .type = TK_CPA, .line_idx = line_idx };
                    break;
            }
        } else if (!isspace((unsigned char)c)) {
            fprintf(stderr, "syntax error: invalid character '%c' on line %zu\n", c, line_idx);
            free(src_buffer);
            return 1;
        }

        line_end_idx = line_idx;
        if (c == '\n') line_idx++;
        src_idx++;
    }

    if (reading_int) {
        Token tkn = {
            .type = TK_INT,
            .int_value = int_value,
            .line_idx = line_int_idx,
        };
        tkn_array[tkn_arr_idx++] = tkn;
    }
    tkn_array[tkn_arr_idx++] = (Token){ .type = TK_EOF, .line_idx = line_end_idx };
    free(src_buffer);

    for (int i = 0; tkn_array[i].type != TK_EOF; i++) {
        switch (tkn_array[i].type) {
            case TK_INT: printf("L%zu  INT(%d)\n", tkn_array[i].line_idx, tkn_array[i].int_value); break;
            case TK_ADD: printf("L%zu  ADD\n", tkn_array[i].line_idx); break;
            case TK_SUB: printf("L%zu  SUB\n", tkn_array[i].line_idx); break;
            case TK_OPA: printf("L%zu  OPA\n", tkn_array[i].line_idx); break;
            case TK_CPA: printf("L%zu  CPA\n", tkn_array[i].line_idx); break;
            default: break;
        }
    }

    return 0;
}
