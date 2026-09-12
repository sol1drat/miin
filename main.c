/*
 * expr    -> mul (("+" | "-") mul)*
 * mul     -> unary (("*" | "/" | "%") unary)*
 * unary   -> ("+" | "-") unary | powr
 * powr    -> primary ("^" powr)?
 * primary -> "(" expr ")" | NUMBER
 */

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
    ND_MUL,
    ND_DIV,
    ND_MOD,
    ND_PWR,
    ND_NEG
} NodeKind;

typedef enum {
    TK_INT,
    TK_ADD,
    TK_SUB,
    TK_MUL,
    TK_DIV,
    TK_MOD,
    TK_PWR,
    TK_OPA,
    TK_CPA,
    TK_EOF
} TokenKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    int value;
    Node *lhs, *rhs;
};

typedef struct {
    TokenKind kind;
    int int_value;
    size_t line_idx;
} Token;

typedef struct {
    Token *toks;
    int pos;
} Parser;

Node *expr(Parser *p);
Node *mul(Parser *p);
Node *unary(Parser *p);
Node *powr(Parser *p);
Node *primary(Parser *p);

bool isopr(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^';
}

bool ispar(char c) {
    return c == '(' || c == ')';
}

const char *kind_name(TokenKind k) {
    switch (k) {
        case TK_INT: return "a number";
        case TK_ADD: return "'+'";
        case TK_SUB: return "'-'";
        case TK_MUL: return "'*'";
        case TK_DIV: return "'/'";
        case TK_MOD: return "'%'";
        case TK_PWR: return "'^'";
        case TK_OPA: return "'('";
        case TK_CPA: return "')'";
        case TK_EOF: return "end of input";
        default: return "?";
    }
}

void dump(Node *n) {
    switch (n->kind) {
        case ND_INT: printf("%d", n->value); return;
        case ND_ADD: printf("(+ "); break;
        case ND_SUB: printf("(- "); break;
        case ND_MUL: printf("(* "); break;
        case ND_DIV: printf("(/ "); break;
        case ND_MOD: printf("(%% "); break;
        case ND_PWR: printf("(^ "); break;
        case ND_NEG: printf("(- "); dump(n->lhs); printf(")"); return;
        default: return;
    }
    dump(n->lhs);
    printf(" ");
    dump(n->rhs);
    printf(")");
}

Node *new_unary(NodeKind kind, Node *lhs) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->lhs = lhs;
    return n;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

Node *new_int(int v) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = ND_INT;
    n->value = v;
    return n;
}

Token *peek(Parser *p) {
    return &p->toks[p->pos];
}

bool match(Parser *p, TokenKind kind) {
    if (peek(p)->kind != kind) return false;
    p->pos++;
    return true;
}

Token *expect(Parser *p, TokenKind kind) {
    if (!match(p, kind)) {
        Token *t = peek(p);
        fprintf(stderr, "\x1b[1;31msyntax error\x1b[0m: expected %s, got %s on line %zu\n",
                kind_name(kind), kind_name(t->kind), t->line_idx);
        exit(1);
    }
    return &p->toks[p->pos - 1];
}

Node *expr(Parser *p) {
    Node *node = mul(p);
    for (;;) {
        if (match(p, TK_ADD)) node = new_binary(ND_ADD, node, mul(p));
        else if (match(p, TK_SUB)) node = new_binary(ND_SUB, node, mul(p));
        else return node;
    }
}

Node *mul(Parser *p) {
    Node *node = unary(p);
    for (;;) {
        if (match(p, TK_MUL)) node = new_binary(ND_MUL, node, unary(p));
        else if (match(p, TK_DIV)) node = new_binary(ND_DIV, node, unary(p));
        else if (match(p, TK_MOD)) node = new_binary(ND_MOD, node, unary(p));
        else return node;
    }
}

Node *unary(Parser *p) {
    if (match(p, TK_ADD)) return unary(p);
    if (match(p, TK_SUB)) return new_unary(ND_NEG, unary(p));
    return powr(p);
}

Node *powr(Parser *p) {
    Node *node = primary(p);
    if (match(p, TK_PWR)) return new_binary(ND_PWR, node, powr(p));
    return node;
}

Node *primary(Parser *p) {
    if (match(p, TK_OPA)) {
        Node *n = expr(p);
        expect(p, TK_CPA);
        return n;
    }
    Token *t = expect(p, TK_INT);
    return new_int(t->int_value);
}

Node *parse(Token *toks) {
    Parser p = { toks, 0 };
    Node *n = expr(&p);
    expect(&p, TK_EOF);
    return n;
}

int ipow(int base, int exp) {
    int result = 1;
    while (exp > 0) {
        if (exp & 1)
            result *= base;

        base *= base;
        exp >>= 1;
    }
    return result;
}

int eval(Node *n) {
    switch (n->kind) {
        case ND_INT: return n->value;
        case ND_ADD: return eval(n->lhs) + eval(n->rhs);
        case ND_SUB: return eval(n->lhs) - eval(n->rhs);
        case ND_MUL: return eval(n->lhs) * eval(n->rhs);
        case ND_DIV: {
            int rhs = eval(n->rhs);
            if (rhs == 0) {
                fprintf(stderr, "\x1b[1;31mruntime error\x1b[0;0m: division by zero\n");
                exit(1);
            }
            return eval(n->lhs) / rhs; 
        }
        case ND_MOD: {
            int rhs = eval(n->rhs);
            if (rhs == 0) {
                fprintf(stderr, "\x1b[1;31mruntime error\x1b[0;0m: modulo by zero\n");
                exit(1);
            }
            return eval(n->lhs) % rhs; 
        }
        case ND_PWR: {
            int rhs = eval(n->rhs);
            if (rhs < 0) {
                fprintf(stderr, "\x1b[1;31mruntime error\x1b[0;0m: exponent less than zero\n");
                exit(1);
            }
            return ipow(eval(n->lhs), rhs); 
        }
        case ND_NEG: return -eval(n->lhs);
        default: abort();
    }
}

char *srcread(const char *file_path) {
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        perror("\x1b\[1;31merror\x1b\[0m: \x1b\[1;37mopening file\x1b\[1;0m");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);

    long size = ftell(fp);
    if (size == -1) {
        perror("\x1b\[1;31merror\x1b\[0m: \x1b\[1;37mgetting file size\x1b\[0m");
        fclose(fp);
        return NULL;
    }
    size_t file_size = (size_t)size;

    rewind(fp);

    char *src_buffer = malloc(file_size + 1);
    if (src_buffer == NULL) {
        perror("\x1b\[1;31merror\x1b\[0m: \x1b\[1;37mallocating memory\x1b\[0m");
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(src_buffer, 1, file_size, fp);
    if (bytes_read != file_size) {
        fprintf(stderr, "\x1b\[1;31merror\x1b\[0m: \x1b\[1;37mreading file\x1b\[0m\n");
        free(src_buffer);
        fclose(fp);
        return NULL;
    }

    src_buffer[file_size] = '\0';
    fclose(fp);
    return src_buffer;
}

int main(int argc, char *argv[]) {
    bool opt_lex = false;
    bool opt_ast = false;
    const char *src_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--lex") == 0) {
            opt_lex = true;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--ast") == 0) {
            opt_ast = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf(
                    "usage: miin [options] file\n"
                    "\n"
                    "miscellaneous interpreter\n"
                    "\n"
                    "options:\n"
                    "  -l, --lex     print lexical tokens and exit\n"
                    "  -a, --ast     print the abstract syntax tree and exit\n"
                    "  -h, --help    show this help message and exit\n"
                  );
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "\x1b\[1;31merror\x1b\[0m: unknown option '%s'\n", argv[i]);
            printf("use option '-h' or '--help' for help\n");
            return 1;
        } else if (src_file == NULL) {
            src_file = argv[i];
        } else {
            fprintf(stderr, "\x1b\[1;31merror\x1b\[0m: unexpected argument '%s'\n", argv[i]);
            printf("use option '-h' or '--help' for help\n");
            return 1;
        }
    }

    if (src_file == NULL) {
        fprintf(stderr, "\x1b\[1;31merror\x1b\[0m: no input file\n");
        printf("use option '-h' or '--help' for help\n");
        return 1;
    }

    char *src_buffer = srcread(src_file);
    if (src_buffer == NULL) return 1;

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
                .kind = TK_INT,
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
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_ADD, .line_idx = line_idx };
                    break;
                case '-':
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_SUB, .line_idx = line_idx };
                    break;
                case '*':
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_MUL, .line_idx = line_idx };
                    break;
                case '/':
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_DIV, .line_idx = line_idx };
                    break;
                case '%':
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_MOD, .line_idx = line_idx };
                    break;
                case '^':
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_PWR, .line_idx = line_idx };
                    break;
            }
        } else if (ispar(c)) {
            switch (c) {
                case '(':
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_OPA, .line_idx = line_idx };
                    break;
                case ')':
                    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_CPA, .line_idx = line_idx };
                    break;
            }
        } else if (!isspace((unsigned char)c)) {
            fprintf(stderr, "\x1b[1;31msyntax error\x1b[0m: invalid character '%c' on line %zu\n", c, line_idx);
            free(src_buffer);
            return 1;
        }

        line_end_idx = line_idx;
        if (c == '\n') line_idx++;
        src_idx++;
    }

    if (reading_int) {
        Token tkn = {
            .kind = TK_INT,
            .int_value = int_value,
            .line_idx = line_int_idx,
        };
        tkn_array[tkn_arr_idx++] = tkn;
    }
    tkn_array[tkn_arr_idx++] = (Token){ .kind = TK_EOF, .line_idx = line_end_idx };
    free(src_buffer);

    if (opt_lex) {
        for (int i = 0; tkn_array[i].kind != TK_EOF; i++) {
            switch (tkn_array[i].kind) {
                case TK_INT: printf("L%zu  \x1b\[1;37mINT\x1b\[0m  %d\n", tkn_array[i].line_idx, tkn_array[i].int_value); break;
                case TK_ADD: printf("L%zu  \x1b\[1;37mADD\x1b\[0m  +\n", tkn_array[i].line_idx); break;
                case TK_SUB: printf("L%zu  \x1b\[1;37mSUB\x1b\[0m  -\n", tkn_array[i].line_idx); break;
                case TK_MUL: printf("L%zu  \x1b\[1;37mMUL\x1b\[0m  *\n", tkn_array[i].line_idx); break;
                case TK_DIV: printf("L%zu  \x1b\[1;37mDIV\x1b\[0m  /\n", tkn_array[i].line_idx); break;
                case TK_MOD: printf("L%zu  \x1b\[1;37mMOD\x1b\[0m  %%\n", tkn_array[i].line_idx); break;
                case TK_PWR: printf("L%zu  \x1b\[1;37mPWR\x1b\[0m  ^\n", tkn_array[i].line_idx); break;
                case TK_OPA: printf("L%zu  \x1b\[1;37mOPA\x1b\[0m  (\n", tkn_array[i].line_idx); break;
                case TK_CPA: printf("L%zu  \x1b\[1;37mCPA\x1b\[0m  )\n", tkn_array[i].line_idx); break;
                default: break;
            }
        }
        return 0;
    }

    Node *ast = parse(tkn_array);

    if (opt_ast) {
        dump(ast);
        putchar('\n');
        return 0;
    }

    printf("%d\n", eval(ast));

    return 0;
}
