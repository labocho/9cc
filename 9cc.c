#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  TK_NUM = 256, // integer
  TK_EOF,
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

char *user_input;
int pos;

Token tokens[100];

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

int consume(int ty) {
  if (tokens[pos].ty != ty)
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
        error_at(tokens[pos].input,
                 "開きカッコに対応する閉じカッコがありません");
      return node;
    }

    if (tokens[pos].ty == TK_NUM)
      return new_node_num(tokens[pos++].val);

    error_at(tokens[pos].input, "数値でも開きカッコでもないトークンです");
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

Node *expr() {
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

  int i = 0;
  while (*p) {
    // skip spaces
    if (isspace(*p)) {
      p++;
      continue;
    }

    switch (*p) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '(':
    case ')':
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

    if (isdigit(*p)) {
      tokens[i].ty = TK_NUM;
      tokens[i].input = p;
      tokens[i].val = strtol(p, &p, 10);
      i++;
      continue;
    }

    error_at(p, "トークナイズできません");
  }

  tokens[i].ty = TK_EOF;
  tokens[i].input = p;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません");
    return 1;
  }

  user_input = argv[1];
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
