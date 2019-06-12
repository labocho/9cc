#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  TK_NUM = 256, // integer
  TK_EOF,
  TK_LE, // <=
  TK_GE, // >=
  TK_EQ, // ==
  TK_NE, // !=
};

enum {
  ND_NUM = 256,
};

typedef struct {
  int ty;      // token type
  int val;     // value of TK_NUM
  char *input; // token string (for error)
} Token;

typedef struct Node {
  int ty;
  struct Node *lhs;
  struct Node *rhs;
  int val;
} Node;

typedef struct {
  void **data;
  int capacity;
  int len;
} Vector;

char *user_input;
int pos;

Vector *tokens;

Vector *new_vector() {
  Vector *vec = malloc(sizeof(Vector));
  vec->data = malloc(sizeof(void *) * 16);
  vec->capacity = 16;
  vec->len = 0;
  return vec;
}

void vec_push(Vector *vec, void *elem) {
  if (vec->capacity == vec->len) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, sizeof(void *) * vec->capacity);
  }
  vec->data[vec->len++] = elem;
}

void error_at(char *loc, char *msg) {
  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ %s\n", msg);
  exit(1);
}

Node *new_node(int ty, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->ty = ty;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));
  node->ty = ND_NUM;
  node->val = val;
  return node;
}

Token *get_token(int index) {
  return tokens->data[index];
}

int consume(int ty) {
  if (get_token(pos)->ty != ty)
    return 0;
  pos++;
  return 1;
}

Node *expr();

Node *term() {
  for (;;) {

    if (consume('(')) {
      Node *node = expr();
      if (!consume(')'))
        error_at(get_token(pos)->input, "開きカッコに対応する閉じカッコがありません");
      return node;
    }

    if (get_token(pos)->ty == TK_NUM)
      return new_node_num(get_token(pos++)->val);

    error_at(get_token(pos)->input, "数値でも開きカッコでもないトークンです");
  }
}

Node *unary() {
  if (consume('+'))
    return term();
  if (consume('-'))
    return new_node('-', new_node_num(0), term()); // - num -> 0 - num
  return term();
}

Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume('*')) {
      node = new_node('*', node, unary());
    } else if (consume('/')) {
      node = new_node('/', node, unary());
    } else {
      return node;
    }
  }
}

Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume('+')) {
      node = new_node('+', node, mul());
    } else if (consume('-')) {
      node = new_node('-', node, mul());
    } else {
      return node;
    }
  }
}

Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume('<')) {
      node = new_node('<', node, add());
    } else if (consume(TK_LE)) {
      node = new_node(TK_LE, node, add());
    } else if (consume('>')) {
      // a > b -> b < a
      node = new_node('<', add(), node);
    } else if (consume(TK_GE)) {
      // a >= b -> b <= a
      node = new_node(TK_LE, add(), node);
    } else {
      return node;
    }
  }
}

Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume(TK_EQ)) {
      node = new_node(TK_EQ, node, relational());
    } else if (consume(TK_NE)) {
      node = new_node(TK_NE, node, relational());
    } else {
      return node;
    }
  }
}

Node *expr() {
  Node *node = equality();
}

void gen(Node *node) {
  // 0-9
  if (node->ty == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  // +, i, *, /
  gen(node->lhs);
  gen(node->rhs);

  // lhs と rhs の結果を pop して rdi と rax に
  printf("  pop rdi\n");
  printf("  pop rax\n");

  // 計算して rax に
  switch (node->ty) {
  case '+':
    printf("  add rax, rdi\n");
    break;
  case '-':
    printf("  sub rax, rdi\n");
    break;
  case '*':
    // imul は rax * 引数 の上位 64bit を rdx に 下位 64bit を rax にセットする
    printf("  imul rdi\n");
    break;
  case '/':
    // idiv は (rdx << 64) +  rax / 引数を rax にセット
    // cqo は rax を 128bit にして rdx と rax にセット
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  case TK_EQ:
    printf("  cmp rax, rdi\n");  // 比較して結果をフラグレジスタに
    printf("  sete al\n");       // 直前の比較で == が真なら 1 を偽なら 0 を AL (rax の下位 8bit) にセット
    printf("  movzb rax, al\n"); // 上位 64-8bit を 0 に
    break;
  case TK_NE:
    printf("  cmp rax, rdi\n");  // 比較して結果をフラグレジスタに
    printf("  setne al\n");      // 直前の比較で == が真なら 1 を偽なら 0 を AL (rax の下位 8bit) にセット
    printf("  movzb rax, al\n"); // 上位 64-8bit を 0 に
    break;
  case '<':
    printf("  cmp rax, rdi\n");  // 比較して結果をフラグレジスタに
    printf("  setl al\n");       // 直前の比較で < が真なら 1 を偽なら 0 を AL (rax の下位 8bit) にセット
    printf("  movzb rax, al\n"); // 上位 64-8bit を 0 に
    break;
  case TK_LE:
    printf("  cmp rax, rdi\n");  // 比較して結果をフラグレジスタに
    printf("  setle al\n");      // 直前の比較で <= が真なら 1 を偽なら 0 を AL (rax の下位 8bit) にセット
    printf("  movzb rax, al\n"); // 上位 64-8bit を 0 に
    break;
  }

  // rax を push して終わり
  printf("  push rax\n");
}

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void tokenize() {
  char *p = user_input;

  while (*p) {
    Token *token = malloc(sizeof(Token));

    // skip spaces
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (strncmp(p, "==", 2) == 0) {
      token->ty = TK_EQ;
      token->input = p;
      vec_push(tokens, token);
      p += 2;
      continue;
    }

    if (strncmp(p, "!=", 2) == 0) {
      token->ty = TK_NE;
      token->input = p;
      vec_push(tokens, token);
      p += 2;
      continue;
    }

    if (strncmp(p, "<=", 2) == 0) {
      token->ty = TK_LE;
      token->input = p;
      vec_push(tokens, token);
      p += 2;
      continue;
    }

    if (strncmp(p, ">=", 2) == 0) {
      token->ty = TK_GE;
      token->input = p;
      vec_push(tokens, token);
      p += 2;
      continue;
    }

    switch (*p) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '(':
    case ')':
    case '<':
    case '>':
      token->ty = *p;
      token->input = p;
      vec_push(tokens, token);
      p++;
      continue;
    }

    if (isdigit(*p)) {
      token->ty = TK_NUM;
      token->input = p;
      token->val = strtol(p, &p, 10);
      vec_push(tokens, token);
      continue;
    }

    error_at(p, "トークナイズできません");
  }

  Token *token = malloc(sizeof(Token));
  token->ty = TK_EOF;
  token->input = p;
  vec_push(tokens, token);
}

void expect(int line, int expected, int actual) {
  if (expected == actual)
    return;
  fprintf(stderr, "%d: %d expected, but got %d\n", line, expected, actual);
  exit(1);
}

void runtest() {
  Vector *vec = new_vector();
  expect(__LINE__, 0, vec->len);

  for (long i = 0; i < 100; i++)
    vec_push(vec, (void *)i);
  expect(__LINE__, 100, vec->len);
  expect(__LINE__, 0, (long)vec->data[0]);
  expect(__LINE__, 50, (long)vec->data[50]);
  expect(__LINE__, 99, (long)vec->data[99]);

  printf("OK\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません");
    return 1;
  }

  if (strcmp("-test", argv[1]) == 0) {
    runtest();
    return 0;
  }

  user_input = argv[1];
  tokens = new_vector();
  tokenize();
  Node *node = expr();

  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  gen(node);

  printf("  pop rax\n");
  printf("  ret\n");
  return 0;
}
