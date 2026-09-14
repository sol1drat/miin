/*
 * expr    -> mul (("+" | "-") mul)*
 * mul     -> unary (("*" | "/" | "%") unary)*
 * unary   -> ("+" | "-") unary | powr
 * powr    -> primary ("^" unary)?
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
    ND_POW,
    ND_NEG
} NodeType;

typedef enum {
    TK_VAR,
    TK_EQL,
    TK_INT,
    TK_ADD,
    TK_SUB,
    TK_MUL,
    TK_DIV,
    TK_MOD,
    TK_POW,
    TK_OPA,
    TK_CPA,
    TK_EOF
} TokenType;

typedef struct Node Node;
struct Node {
    NodeType type;
    int value;
    Node *lhs, *rhs;
};

typedef struct {
    TokenType type;
    char *var_value;
    size_t var_len;
    int int_value;
    size_t line_num;
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

void *check_alloc(void *p) {
    if (p == NULL) {
        fprintf(stderr, "\x1b[1;31mfatal\x1b[0m: out of memory\n");
        exit(1);
    }
    return p;
}

void *xmalloc(size_t size)           { return check_alloc(malloc(size)); }
void *xcalloc(size_t n, size_t size) { return check_alloc(calloc(n, size)); }
void *xrealloc(void *p, size_t size) { return check_alloc(realloc(p, size)); }

const char *type_name(TokenType t) {
    switch (t) {
        case TK_VAR: return "a variable";
        case TK_INT: return "a number";
        case TK_ADD: return "'+'";
        case TK_SUB: return "'-'";
        case TK_MUL: return "'*'";
        case TK_DIV: return "'/'";
        case TK_MOD: return "'%'";
        case TK_POW: return "'^'";
        case TK_OPA: return "'('";
        case TK_CPA: return "')'";
        case TK_EQL: return "'='";
        case TK_EOF: return "end of input";
        default: return "?";
    }
}

void print_ast(Node *n) {
    switch (n->type) {
        case ND_INT: printf("%d", n->value); return;
        case ND_ADD: printf("(+ "); break;
        case ND_SUB: printf("(- "); break;
        case ND_MUL: printf("(* "); break;
        case ND_DIV: printf("(/ "); break;
        case ND_MOD: printf("(%% "); break;
        case ND_POW: printf("(^ "); break;
        case ND_NEG: printf("(- "); print_ast(n->lhs); printf(")"); return;
        default: return;
    }
    print_ast(n->lhs);
    printf(" ");
    print_ast(n->rhs);
    printf(")");
}

void println_ast(Node *n) {
    print_ast(n);
    putchar('\n');
}

void println_toks(Token *toks) {
    for (int i = 0; toks[i].type != TK_EOF; i++) {
        switch (toks[i].type) {
            case TK_VAR: printf("L%zu  \x1b[1;37mVAR\x1b[0m  %.*s\n", toks[i].line_num, (int)toks[i].var_len, toks[i].var_value); break;
            case TK_INT: printf("L%zu  \x1b[1;37mINT\x1b[0m  %d\n", toks[i].line_num, toks[i].int_value); break;
            case TK_ADD: printf("L%zu  \x1b[1;37mADD\x1b[0m  +\n", toks[i].line_num); break;
            case TK_SUB: printf("L%zu  \x1b[1;37mSUB\x1b[0m  -\n", toks[i].line_num); break;
            case TK_MUL: printf("L%zu  \x1b[1;37mMUL\x1b[0m  *\n", toks[i].line_num); break;
            case TK_DIV: printf("L%zu  \x1b[1;37mDIV\x1b[0m  /\n", toks[i].line_num); break;
            case TK_MOD: printf("L%zu  \x1b[1;37mMOD\x1b[0m  %%\n", toks[i].line_num); break;
            case TK_POW: printf("L%zu  \x1b[1;37mPOW\x1b[0m  ^\n", toks[i].line_num); break;
            case TK_OPA: printf("L%zu  \x1b[1;37mOPA\x1b[0m  (\n", toks[i].line_num); break;
            case TK_CPA: printf("L%zu  \x1b[1;37mCPA\x1b[0m  )\n", toks[i].line_num); break;
            case TK_EQL: printf("L%zu  \x1b[1;37mEQL\x1b[0m  =\n", toks[i].line_num); break;
            default: break;
        }
    }
}

Node *new_unary(NodeType type, Node *lhs) {
    Node *n = xcalloc(1, sizeof(Node));
    n->type = type;
    n->lhs = lhs;
    return n;
}

Node *new_binary(NodeType type, Node *lhs, Node *rhs) {
    Node *n = xcalloc(1, sizeof(Node));
    n->type = type;
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

Node *new_int(int v) {
    Node *n = xcalloc(1, sizeof(Node));
    n->type = ND_INT;
    n->value = v;
    return n;
}

Token *peek(Parser *p) {
    return &p->toks[p->pos];
}

bool match(Parser *p, TokenType type) {
    if (peek(p)->type != type) return false;
    p->pos++;
    return true;
}

Token *expect(Parser *p, TokenType type) {
    if (!match(p, type)) {
        Token *t = peek(p);
        fprintf(stderr, "\x1b[1;31msyntax error\x1b[0m: expected %s, got %s on line %zu\n",
                type_name(type), type_name(t->type), t->line_num);
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
    if (match(p, TK_POW)) return new_binary(ND_POW, node, unary(p));
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

void free_ast(Node *n) {
    if (n == NULL) return;
    free_ast(n->lhs);
    free_ast(n->rhs);
    free(n);
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
    switch (n->type) {
        case ND_INT: return n->value;
        case ND_ADD: return eval(n->lhs) + eval(n->rhs);
        case ND_SUB: return eval(n->lhs) - eval(n->rhs);
        case ND_MUL: return eval(n->lhs) * eval(n->rhs);
        case ND_DIV: {
            int rhs = eval(n->rhs);
            if (rhs == 0) {
                fprintf(stderr, "\x1b[1;31mruntime error\x1b[0m: division by zero\n");
                exit(1);
            }
            return eval(n->lhs) / rhs; 
        }
        case ND_MOD: {
            int rhs = eval(n->rhs);
            if (rhs == 0) {
                fprintf(stderr, "\x1b[1;31mruntime error\x1b[0m: modulo by zero\n");
                exit(1);
            }
            return eval(n->lhs) % rhs; 
        }
        case ND_POW: {
            int rhs = eval(n->rhs);
            if (rhs < 0) {
                fprintf(stderr, "\x1b[1;31mruntime error\x1b[0m: exponent less than zero\n");
                exit(1);
            }
            return ipow(eval(n->lhs), rhs); 
        }
        case ND_NEG: return -eval(n->lhs);
        default: abort();
    }
}

char *file_to_buf(const char *file_path) {
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        perror("\x1b[1;31merror\x1b[0m: \x1b[1;37mopening file\x1b[0m");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);

    long size = ftell(fp);
    if (size == -1) {
        perror("\x1b[1;31merror\x1b[0m: \x1b[1;37mgetting file size\x1b[0m");
        fclose(fp);
        return NULL;
    }
    size_t file_size = (size_t)size;

    rewind(fp);

    char *src = xmalloc(file_size + 1);
    if (src == NULL) {
        perror("\x1b[1;31merror\x1b[0m: \x1b[1;37mallocating memory\x1b[0m");
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(src, 1, file_size, fp);
    if (bytes_read != file_size) {
        fprintf(stderr, "\x1b[1;31merror\x1b[0m: \x1b[1;37mreading file\x1b[0m\n");
        free(src);
        fclose(fp);
        return NULL;
    }

    src[file_size] = '\0';
    fclose(fp);
    return src;
}

Token *push_tok(Token *toks, size_t *len, size_t *cap, Token tok) {
    if (*len == *cap) {
        *cap = *cap ? *cap * 2 : 64;
        Token *tmp = xrealloc(toks, *cap * sizeof(Token));
        if (tmp == NULL) {
            perror("\x1b[1;31merror\x1b[0m: \x1b[1;37mallocating memory\x1b[0m");
            free(toks);
            return NULL;
        }
        toks = tmp;
    }
    toks[(*len)++] = tok;
    return toks;
}

Token *lex(char *src) {
    Token *toks = NULL;
    size_t i = 0, len = 0, cap = 0, line_num = 1;

    while (src[i] != '\0') {
        char c = src[i];

        if (isspace((unsigned char)c)) {
            if (c == '\n') line_num++;
            i++;
            continue;
        }

        // [A-Za-z_][A-Za-z0-9_]*
        if (isalpha((unsigned char)c) || c == '_') {
            size_t s = i;
            while (isalnum((unsigned char)src[i])|| src[i] == '_') i++;
            toks = push_tok(toks, &len, &cap, (Token){
                .type = TK_VAR, .var_value = &src[s],
                .var_len = i-s, .line_num = line_num
            });
            continue;
        }

        if (isdigit((unsigned char)c)) {
            int v = 0;
            while (isdigit((unsigned char)src[i])) v = v * 10 + (src[i++] - '0');
            toks = push_tok(toks, &len, &cap, (Token){
                .type = TK_INT, .int_value = v, .line_num = line_num
            });
            continue;
        }

        TokenType type;
        switch (c) {
            case '+': type = TK_ADD; break;
            case '-': type = TK_SUB; break;
            case '*': type = TK_MUL; break;
            case '/': type = TK_DIV; break;
            case '%': type = TK_MOD; break;
            case '^': type = TK_POW; break;
            case '(': type = TK_OPA; break;
            case ')': type = TK_CPA; break;
            case '=': type = TK_EQL; break;
            default:
                fprintf(stderr, "\x1b[1;31msyntax error\x1b[0m: invalid character '%c' on line %zu\n", c, line_num);
                free(toks);
                return NULL;
        }

        toks = push_tok(toks, &len, &cap, (Token){ .type = type, .line_num = line_num });
        i++;
    }

    toks = push_tok(toks, &len, &cap, (Token){ .type = TK_EOF, .line_num = line_num });
    return toks;
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
            fprintf(stderr, "\x1b[1;31merror\x1b[0m: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "use option '-h' or '--help' for help\n");
            return 1;
        } else if (src_file == NULL) {
            src_file = argv[i];
        } else {
            fprintf(stderr, "\x1b[1;31merror\x1b[0m: unexpected argument '%s'\n", argv[i]);
            fprintf(stderr, "use option '-h' or '--help' for help\n");
            return 1;
        }
    }

    if (src_file == NULL) {
        fprintf(stderr, "\x1b[1;31merror\x1b[0m: no input file\n");
        fprintf(stderr, "use option '-h' or '--help' for help\n");
        return 1;
    }

    char *src = file_to_buf(src_file);
    if (src == NULL) return 1;

    Token *toks = lex(src);
    if (toks == NULL) {
        free(src);
        return 1;
    }

    if (opt_lex) {
        println_toks(toks);
        free(toks);
        free(src);
        return 0;
    }

    Node *ast = parse(toks);
    free(toks);
    free(src);

    if (opt_ast) {
        println_ast(ast);
        free_ast(ast);
        return 0;
    }

    printf("%d\n", eval(ast));
    free_ast(ast);

    return 0;
}
