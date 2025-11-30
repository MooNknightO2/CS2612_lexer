#ifndef LANG_H_INCLUDED
#define LANG_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

struct char_set {
  char * c;
  unsigned int n;
};

enum FrontendRegExpType {
  T_FR_CHAR_SET = 0,
  T_FR_OPTIONAL,
  T_FR_STAR,
  T_FR_PLUS,
  T_FR_STRING,
  T_FR_SINGLE_CHAR,
  T_FR_UNION,
  T_FR_CONCAT
};

struct frontend_regexp {
  enum FrontendRegExpType t;
  union {
    struct char_set CHAR_SET;
    struct {struct frontend_regexp * r; } OPTION;
    struct {struct frontend_regexp * r; } STAR;
    struct {struct frontend_regexp * r; } PLUS;
    struct {char * s; } STRING;
    struct {char c; } SINGLE_CHAR;
    struct {struct frontend_regexp * r1; struct frontend_regexp * r2; } UNION;
    struct {struct frontend_regexp * r1; struct frontend_regexp * r2; } CONCAT;
  } d;
};

enum SimplRegExpType {
  T_S_CHAR_SET = 0,
  T_S_STAR,
  T_S_EMPTY_STR,
  T_S_UNION,
  T_S_CONCAT
};

struct simpl_regexp {
  enum SimplRegExpType t;
  union {
    struct char_set CHAR_SET;
    struct {struct simpl_regexp * r; } STAR;
    struct {void * none; } EMPTY_STR;
    struct {struct simpl_regexp * r1; struct simpl_regexp * r2; } UNION;
    struct {struct simpl_regexp * r1; struct simpl_regexp * r2; } CONCAT;
  } d;
};

struct finite_automata {
  int n; /* number of vertices, id of vertices are: 0, 1, ..., (n - 1) */
  int m; /* number of vertices, id of vertices are: 0, 1, ..., (n - 1) */
  int * src; /* for every edge e, src[e] is the source vertex of e */
  int * dst; /* for every edge e, dst[e] is the destination vertex of e */
  struct char_set * lb; /* for every edge e, lb[e] are the transition lables on e, if the char set empty, the edge is an epsilon edge */
};

void copy_char_set(struct char_set * dst, struct char_set * src);
struct frontend_regexp * TFr_CharSet(struct char_set * c);
struct frontend_regexp * TFr_Option(struct frontend_regexp * r);
struct frontend_regexp * TFr_Star(struct frontend_regexp * r);
struct frontend_regexp * TFr_Plus(struct frontend_regexp * r);
struct frontend_regexp * TFr_String(char * s);
struct frontend_regexp * TFr_SingleChar(char c);
struct frontend_regexp * TFr_Union(struct frontend_regexp * r1, struct frontend_regexp * r2);
struct frontend_regexp * TFr_Concat(struct frontend_regexp * r1, struct frontend_regexp * r2);
struct simpl_regexp * TS_CharSet(struct char_set * c);
struct simpl_regexp * TS_Star(struct simpl_regexp * r);
struct simpl_regexp * TS_EmptyStr();
struct simpl_regexp * TS_Union(struct simpl_regexp * r1, struct simpl_regexp * r2);
struct simpl_regexp * TS_Concat(struct simpl_regexp * r1, struct simpl_regexp * r2);
struct finite_automata * create_empty_graph();
int add_one_vertex(struct finite_automata * g); /* add a new vertex to the graph and return the id of the new vertex */
int add_one_edge(struct finite_automata * g, int src, int dst, struct char_set * c); /* add a new edge to the graph and return the id of the new edge */

#endif // LANG_H_INCLUDED
