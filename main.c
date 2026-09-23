/*
 * program -> stmt* EOF
 * stmt    -> "if" cmp block ("else" block)?
              | "print" cmp
              | cmp ("=" cmp)?
 * block   -> "{" stmt* "}"
 * cmp     -> expr (("==" | "!=" | "<" | "<=" | ">" | ">=") expr)?
 * expr    -> mul (("+" | "-") mul)*
 * mul     -> unary (("*" | "/" | "%") unary)*
 * unary   -> ("+" | "-") unary | powr
 * powr    -> primary ("^" unary)?
 * primary -> "(" cmp ")" | NUMBER | IDENT | "true" | "false"
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    ND_IF,
    ND_BLOCK,
    ND_EQ,
    ND_NE,
    ND_LT,
    ND_LE,
    ND_GT,
    ND_GE,
    ND_PRINT,
    ND_VAR,
    ND_ASSIGN,
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
    TK_EQ,
    TK_NE,
    TK_LT,
    TK_LE,
    TK_GT,
    TK_GE,
    TK_OB,
    TK_CB,
    TK_VAR,
    TK_PRINT,
    TK_TRUE,
    TK_FALSE,
    TK_IF,
    TK_ELSE,
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
    TK_EOF,
    TK_EOL
} TokenType;

typedef struct Node Node;
struct Node {
    NodeType type;
    int value;
    char *name;
    Node *lhs, *rhs;
    Node *els;
    Node **body;
    size_t body_len;
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

typedef struct {
    const char *word;
    TokenType type;
} Keyword;

const Keyword keywords[] = {
    { "print", TK_PRINT },
    { "if", TK_IF},
    { "else", TK_ELSE},
    { "true", TK_TRUE},
    { "false", TK_FALSE},
};

typedef struct {
    char *name;
    int value;
} Var;

typedef struct {
    Var *vars;
    size_t len, cap;
} Env;

typedef struct {
    Node **stmts;
    size_t len;
} Program;

Node *expr(Parser *p);
Node *stmt(Parser *p);
Node *cmp(Parser *p);
Node *block(Parser *p);
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
char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = xmalloc(n);
    memcpy(p, s, n);
    return p;
}

const char *type_name(TokenType t) {
    switch (t) {
        case TK_PRINT: return "'print'";
        case TK_TRUE:  return "'true'";
        case TK_FALSE: return "'false'";
        case TK_IF:    return "'if'";
        case TK_ELSE:  return "'else'";
        case TK_LT:    return "'<'";
        case TK_GT:    return "'>'";
        case TK_OB:    return "'{'";
        case TK_CB:    return "'}'";
        case TK_EQ:    return "'=='";
        case TK_NE:    return "'!='";
        case TK_LE:    return "'<='";
        case TK_GE:    return "'>='";
        case TK_VAR:   return "a variable";
        case TK_INT:   return "a number";
        case TK_ADD:   return "'+'";
        case TK_SUB:   return "'-'";
        case TK_MUL:   return "'*'";
        case TK_DIV:   return "'/'";
        case TK_MOD:   return "'%'";
        case TK_POW:   return "'^'";
        case TK_OPA:   return "'('";
        case TK_CPA:   return "')'";
        case TK_EQL:   return "'='";
        case TK_EOF:   return "end of input";
        case TK_EOL:   return "end of line";
        default:       return "?";
    }
}

void print_ast(Node *n) {
    switch (n->type) {
        case ND_PRINT:  printf("(print "); print_ast(n->lhs); printf(")"); return;
        case ND_VAR:    printf("%s", n->name); return;
        case ND_INT:    printf("%d", n->value); return;
        case ND_ASSIGN: printf("(= "); break;
        case ND_ADD:    printf("(+ "); break;
        case ND_SUB:    printf("(- "); break;
        case ND_MUL:    printf("(* "); break;
        case ND_DIV:    printf("(/ "); break;
        case ND_EQ:     printf("(== "); break;
        case ND_NE:     printf("(!= "); break;
        case ND_LT:     printf("(< ");  break;
        case ND_LE:     printf("(<= "); break;
        case ND_GT:     printf("(> ");  break;
        case ND_GE:     printf("(>= "); break;
        case ND_MOD:    printf("(%% "); break;
        case ND_POW:    printf("(^ "); break;
        case ND_NEG:    printf("(- "); print_ast(n->lhs); printf(")"); return;
        case ND_IF: {
            printf("(if ");
            print_ast(n->lhs); printf(" ");
            print_ast(n->rhs);
            if (n->els != NULL) { printf(" "); print_ast(n->els); }
            printf(")");
            return;
        }
        case ND_BLOCK: {
            printf("(block");
            for (size_t i = 0; i < n->body_len; i++) {
                printf(" ");
                print_ast(n->body[i]);
            }
            printf(")");
            return;
        }
        default: abort();
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
            case TK_IF: printf("L%zu  \x1b[1;37mIF\x1b[0m   %.*s\n", toks[i].line_num, (int)toks[i].var_len, toks[i].var_value); break;
            case TK_ELSE: printf("L%zu  \x1b[1;37mELS\x1b[0m  else\n", toks[i].line_num); break;
            case TK_TRUE: printf("L%zu  \x1b[1;37mTRU\x1b[0m  true\n", toks[i].line_num); break;
            case TK_FALSE: printf("L%zu  \x1b[1;37mFAL\x1b[0m  false\n", toks[i].line_num); break;
            case TK_LT: printf("L%zu  \x1b[1;37mLT\x1b[0m   '<'\n", toks[i].line_num); break;
            case TK_GT: printf("L%zu  \x1b[1;37mGT\x1b[0m   '>'\n", toks[i].line_num); break;
            case TK_OB: printf("L%zu  \x1b[1;37mOB\x1b[0m   '{'\n", toks[i].line_num); break;
            case TK_CB: printf("L%zu  \x1b[1;37mCB\x1b[0m   '}'\n", toks[i].line_num); break;
            case TK_LE: printf("L%zu  \x1b[1;37mLE\x1b[0m   '<='\n", toks[i].line_num); break;
            case TK_GE: printf("L%zu  \x1b[1;37mGE\x1b[0m   '>='\n", toks[i].line_num); break;
            case TK_EQ: printf("L%zu  \x1b[1;37mEQ\x1b[0m   '=='\n", toks[i].line_num); break;
            case TK_NE: printf("L%zu  \x1b[1;37mNE\x1b[0m   '!='\n", toks[i].line_num); break;
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
            case TK_EOL: printf("L%zu  \x1b[1;37mEOL\x1b[0m  \\n\n", toks[i].line_num); break;
            case TK_PRINT: printf("L%zu  \x1b[1;37mPRT\x1b[0m  %.*s\n", toks[i].line_num, (int)toks[i].var_len, toks[i].var_value); break;
            default: break;
        }
    }
}

Var *env_find(Env *e, const char *name) {
    for (size_t i = 0; i < e->len; i++)
        if (strcmp(e->vars[i].name, name) == 0)
            return &e->vars[i];
    return NULL;
}

void env_set(Env *e, const char *name, int value) {
    Var *v = env_find(e, name);
    if (v != NULL) {
        v->value = value;
        return;
    }
    if (e->len == e->cap) {
        e->cap = e->cap * 2 + 8;
        e->vars = xrealloc(e->vars, e->cap * sizeof(Var));
    }
    e->vars[e->len].name = xstrdup(name);
    e->vars[e->len].value = value;
    e->len++;
}

void env_free(Env *e) {
    for (size_t i = 0; i < e->len; i++)
        free(e->vars[i].name);
    free(e->vars);
}

Node *new_block(Node **stmts, size_t len) {
    Node *n = xcalloc(1, sizeof(Node));
    n->type = ND_BLOCK;
    n->body = stmts;
    n->body_len = len;
    return n;
}

Node *new_if(Node *cond, Node *then_blk, Node *else_blk) {
    Node *n = xcalloc(1, sizeof(Node));
    n->type = ND_IF;
    n->lhs = cond;
    n->rhs = then_blk;
    n->els = else_blk;
    return n;
}

Node *new_var(const char *name, size_t len) {
    Node *n = xcalloc(1, sizeof(Node));
    n->type = ND_VAR;
    n->name = xmalloc(len + 1);
    memcpy(n->name, name, len);
    n->name[len] = '\0';
    return n;
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

Node *stmt(Parser *p) {
   if (match(p, TK_IF)) {
        Node *cond = cmp(p);
        Node *then_blk = block(p);
        Node *else_blk = NULL;
        if (match(p, TK_ELSE)) {
            if (peek(p)->type == TK_IF)
                else_blk = stmt(p);
            else
                else_blk = block(p);
        }
        return new_if(cond, then_blk, else_blk);
    }
    if (match(p, TK_PRINT))
        return new_unary(ND_PRINT, cmp(p));
    Node *node = cmp(p);
    Token *t = peek(p);
    if (t->type != TK_EQL)
        return node;
    if (node->type != ND_VAR) {
        fprintf(stderr, "\x1b[1;31msyntax error\x1b[0m: cannot assign on line %zu\n", t->line_num);
        exit(1);
    }
    p->pos++;
    return new_binary(ND_ASSIGN, node, cmp(p));
}

Node *cmp(Parser *p) {
    Node *node = expr(p);
    TokenType t = peek(p)->type;
    if (t != TK_EQ && t != TK_NE && t != TK_LT &&
        t != TK_LE && t != TK_GT && t != TK_GE)
        return node;
    p->pos++;
    NodeType type;
    switch (t) {
        case TK_EQ: type = ND_EQ; break;
        case TK_NE: type = ND_NE; break;
        case TK_LT: type = ND_LT; break;
        case TK_LE: type = ND_LE; break;
        case TK_GT: type = ND_GT; break;
        default:    type = ND_GE; break;
    }
    return new_binary(type, node, expr(p));
}

Node *block(Parser *p) {
    expect(p, TK_OB);
    Node **stmts = NULL;
    size_t len = 0, cap = 0;
    for (;;) {
        while (match(p, TK_EOL));
        if (match(p, TK_CB)) break;

        if (len == cap) {
            cap = cap * 2 + 8;
            stmts = xrealloc(stmts, cap * sizeof(Node *));
        }
        stmts[len++] = stmt(p);

        if (!match(p, TK_EOL)) {
            expect(p, TK_CB);
            break;
        }
    }
    return new_block(stmts, len);
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
        Node *n = cmp(p);
        expect(p, TK_CPA);
        return n;
    }
    if (match(p, TK_TRUE))  return new_int(1);
    if (match(p, TK_FALSE)) return new_int(0);
    Token *t = peek(p);
    if (match(p, TK_VAR)) return new_var(t->var_value, t->var_len);
    t = expect(p, TK_INT);
    return new_int(t->int_value);
}

Program parse(Token *toks) {
    Program prog = { NULL, 0 };
    Parser p = { toks, 0 };
    size_t cap = 0;
    for (;;) {
        while (match(&p, TK_EOL)) continue;
        if (peek(&p)->type == TK_EOF) break;

        if (prog.len == cap) {
            cap = cap * 2 + 8;
            prog.stmts = xrealloc(prog.stmts, cap * sizeof(Node*));
        }
        prog.stmts[prog.len++] = stmt(&p);

        if (!match(&p, TK_EOL)) {
            expect(&p, TK_EOF);
            break;
        }
    }
    return prog;
}

void free_ast(Node *n) {
    if (n == NULL) return;
    free_ast(n->lhs);
    free_ast(n->rhs);
    free_ast(n->els);
    for (size_t i = 0; i < n->body_len; i++)
        free_ast(n->body[i]);
    free(n->body);
    free(n->name);
    free(n);
}

void free_prog(Program *prog) {
    for (size_t i = 0; i < prog->len; i++)
        free_ast(prog->stmts[i]);
    free(prog->stmts);
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

int eval(Node *n, Env *env) {
    switch (n->type) {
        case ND_IF: {
            if (eval(n->lhs, env) != 0)       // truthiness: nonzero = true
                return eval(n->rhs, env);
            if (n->els != NULL)
                return eval(n->els, env);
            return 0;
        }
        case ND_BLOCK: {
            int result = 0;
            for (size_t i = 0; i < n->body_len; i++)
                result = eval(n->body[i], env);
            return result;
        }
        case ND_EQ: return eval(n->lhs, env) == eval(n->rhs, env);
        case ND_NE: return eval(n->lhs, env) != eval(n->rhs, env);
        case ND_LT: return eval(n->lhs, env) <  eval(n->rhs, env);
        case ND_LE: return eval(n->lhs, env) <= eval(n->rhs, env);
        case ND_GT: return eval(n->lhs, env) >  eval(n->rhs, env);
        case ND_GE: return eval(n->lhs, env) >= eval(n->rhs, env);
        case ND_PRINT: {
            int value = eval(n->lhs, env);
            printf("%d\n", value);
            return value;
        }
        case ND_INT: return n->value;
        case ND_VAR: {
            Var *v = env_find(env, n->name);
            if (v == NULL) {
                fprintf(stderr, "\x1b[1;31mruntime error\x1b[0m: undefined variable '%s'\n", n->name);
                exit(1);
            }
            return v->value;
        }
        case ND_ASSIGN: {
            int value = eval(n->rhs, env);
            env_set(env, n->lhs->name, value);
            return value;
        }
        case ND_ADD: return eval(n->lhs, env) + eval(n->rhs, env);
        case ND_SUB: return eval(n->lhs, env) - eval(n->rhs, env);
        case ND_MUL: return eval(n->lhs, env) * eval(n->rhs, env);
        case ND_DIV: {
            int rhs = eval(n->rhs, env);
            if (rhs == 0) { fprintf(stderr, "\x1b[1;31mruntime error\x1b[0m: division by zero\n"); exit(1); }
            return eval(n->lhs, env) / rhs;
        }
        case ND_MOD: {
            int rhs = eval(n->rhs, env);
            if (rhs == 0) { fprintf(stderr, "\x1b[1;31mruntime error\x1b[0m: modulo by zero\n"); exit(1); }
            return eval(n->lhs, env) % rhs;
        }
        case ND_POW: {
            int rhs = eval(n->rhs, env);
            if (rhs < 0) { fprintf(stderr, "\x1b[1;31mruntime error\x1b[0m: exponent less than zero\n"); exit(1); }
            return ipow(eval(n->lhs, env), rhs);
        }
        case ND_NEG: return -eval(n->lhs, env);
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
        *cap = *cap * 2 + 64;
        Token *tmp = xrealloc(toks, *cap * sizeof(Token));
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
            if (c == '\n') {
                toks = push_tok(toks, &len, &cap, (Token){ .type = TK_EOL, .line_num = line_num });
                line_num++;
            }
            i++;
            continue;
        }

        // [a-zA-Z_][a-zA-Z0-9_]*
        if (isalpha((unsigned char)c) || c == '_') {
            size_t s = i;
            while (isalnum((unsigned char)src[i])|| src[i] == '_') i++;
            size_t n = i-s;
            TokenType type = TK_VAR;
            for (size_t k = 0; k < sizeof(keywords) / sizeof(keywords[0]); k++) {
                if (n == strlen(keywords[k].word) &&
                        memcmp(&src[s], keywords[k].word, n) == 0) {
                    type = keywords[k].type;
                    break;
                }
            }
            toks = push_tok(toks, &len, &cap, (Token){
                .type = type, .var_value = &src[s],
                .var_len = n, .line_num = line_num
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

        if ((c == '=' || c == '!' || c == '<' || c == '>') && src[i + 1] == '=') {
            TokenType type;
            switch (c) {
                case '=': type = TK_EQ; break;   // ==
                case '!': type = TK_NE; break;   // !=
                case '<': type = TK_LE; break;   // <=
                default:  type = TK_GE; break;   // >=
            }
            toks = push_tok(toks, &len, &cap, (Token){ .type = type, .line_num = line_num });
            i += 2;
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
            case '<': type = TK_LT; break;
            case '>': type = TK_GT; break;
            case '{': type = TK_OB; break;
            case '}': type = TK_CB; break;
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

    Program prog = parse(toks);
    free(toks);
    free(src);

    if (opt_ast) {
        for (size_t i = 0; i < prog.len; i++) println_ast(prog.stmts[i]);
        free_prog(&prog);
        return 0;
    }

    Env env = { NULL, 0, 0 };
    for (size_t i = 0; i < prog.len; i++) eval(prog.stmts[i], &env);

    env_free(&env);
    free_prog(&prog);
    return 0;
}
