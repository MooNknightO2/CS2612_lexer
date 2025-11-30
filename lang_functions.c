#include "lang.h"
#include <stdlib.h>
#include <string.h>

// 字符集操作函数
void copy_char_set(struct char_set * dst, struct char_set * src) {
    if (!dst || !src) return;
    dst->n = src->n;
    dst->c = malloc(src->n * sizeof(char));
    memcpy(dst->c, src->c, src->n * sizeof(char));
}

// 前端正则表达式构造
struct frontend_regexp * TFr_CharSet(struct char_set * c) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_CHAR_SET;
    copy_char_set(&fr->d.CHAR_SET, c);
    free(c->c);
    free(c);
    return fr;
}

struct frontend_regexp * TFr_Option(struct frontend_regexp * r) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_OPTIONAL;
    fr->d.OPTION.r = r;
    return fr;
}

struct frontend_regexp * TFr_Star(struct frontend_regexp * r) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_STAR;
    fr->d.STAR.r = r;
    return fr;
}

struct frontend_regexp * TFr_Plus(struct frontend_regexp * r) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_PLUS;
    fr->d.PLUS.r = r;
    return fr;
}

struct frontend_regexp * TFr_String(char * s) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_STRING;
    fr->d.STRING.s = strdup(s);
    return fr;
}

struct frontend_regexp * TFr_SingleChar(char c) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_SINGLE_CHAR;
    fr->d.SINGLE_CHAR.c = c;
    return fr;
}

struct frontend_regexp * TFr_Union(struct frontend_regexp * r1, struct frontend_regexp * r2) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_UNION;
    fr->d.UNION.r1 = r1;
    fr->d.UNION.r2 = r2;
    return fr;
}

struct frontend_regexp * TFr_Concat(struct frontend_regexp * r1, struct frontend_regexp * r2) {
    struct frontend_regexp * fr = malloc(sizeof(struct frontend_regexp));
    fr->t = T_FR_CONCAT;
    fr->d.CONCAT.r1 = r1;
    fr->d.CONCAT.r2 = r2;
    return fr;
}

// 简化正则表达式构造
struct simpl_regexp * TS_CharSet(struct char_set * c) {
    struct simpl_regexp * sr = malloc(sizeof(struct simpl_regexp));
    sr->t = T_S_CHAR_SET;
    copy_char_set(&sr->d.CHAR_SET, c);
    free(c->c);
    free(c);
    return sr;
}

struct simpl_regexp * TS_Star(struct simpl_regexp * r) {
    struct simpl_regexp * sr = malloc(sizeof(struct simpl_regexp));
    sr->t = T_S_STAR;
    sr->d.STAR.r = r;
    return sr;
}

struct simpl_regexp * TS_EmptyStr() {
    struct simpl_regexp * sr = malloc(sizeof(struct simpl_regexp));
    sr->t = T_S_EMPTY_STR;
    return sr;
}

struct simpl_regexp * TS_Union(struct simpl_regexp * r1, struct simpl_regexp * r2) {
    struct simpl_regexp * sr = malloc(sizeof(struct simpl_regexp));
    sr->t = T_S_UNION;
    sr->d.UNION.r1 = r1;
    sr->d.UNION.r2 = r2;
    return sr;
}

struct simpl_regexp * TS_Concat(struct simpl_regexp * r1, struct simpl_regexp * r2) {
    struct simpl_regexp * sr = malloc(sizeof(struct simpl_regexp));
    sr->t = T_S_CONCAT;
    sr->d.CONCAT.r1 = r1;
    sr->d.CONCAT.r2 = r2;
    return sr;
}

// 有限自动机操作
struct finite_automata * create_empty_graph() {
    struct finite_automata * fa = malloc(sizeof(struct finite_automata));
    fa->n = 0;
    fa->m = 0;
    fa->src = NULL;
    fa->dst = NULL;
    fa->lb = NULL;
    return fa;
}

int add_one_vertex(struct finite_automata * g) {
    g->n++;
    return g->n - 1; // 返回新加的那个点的编号
}

int add_one_edge(struct finite_automata * g, int src, int dst, struct char_set * c) {
    g->m++;
    g->src = realloc(g->src, g->m * sizeof(int));
    g->dst = realloc(g->dst, g->m * sizeof(int));
    g->lb = realloc(g->lb, g->m * sizeof(struct char_set));
    
    g->src[g->m - 1] = src;
    g->dst[g->m - 1] = dst;
    
    if (c) {
        copy_char_set(&g->lb[g->m - 1], c);
    } else {
        g->lb[g->m - 1].n = 0;
        g->lb[g->m - 1].c = NULL;
    }
    
    return g->m - 1;
}