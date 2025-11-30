#ifndef LEXER_H_INCLUDED
#define LEXER_H_INCLUDED

#include "lang.h"
#include <stdbool.h>

// 状态集合结构
typedef struct {
    int* states;
    int size;
    int id; // DFA状态ID
} StateSet;

// NFA片段结构
typedef struct {
    int start;
    int end;
} NFAFragment;

// 词法分析器结构
struct Lexer {
    struct finite_automata* dfa;
    int* dfa_accepting_rules;
    int dfa_size;
};

// ==================== 字符集操作函数 ====================
int char_in_set(char c, struct char_set* cs);
struct char_set* create_char_set_from_range(char start, char end);
struct char_set* create_char_set_from_chars(char* chars, int n);

// ==================== 状态集合操作函数 ====================
StateSet* create_state_set(int* states, int size, int id);
void free_state_set(StateSet* set);
int state_set_equal(StateSet* a, StateSet* b);
void sort_state_set(StateSet* set);

// ==================== 正则表达式简化函数 ====================
struct simpl_regexp* simplify_regexp(struct frontend_regexp* fr);

// ==================== NFA构建函数 ====================
NFAFragment regexp_to_nfa_fragment(struct finite_automata* nfa, struct simpl_regexp* sr);
struct finite_automata* build_nfa_from_regexp(struct simpl_regexp* sr);

// ==================== ε-闭包计算函数 ====================
void epsilon_closure(struct finite_automata* nfa, int state, int* visited, int* closure, int* closure_size);
int* get_epsilon_closure(struct finite_automata* nfa, int state, int* size);

// ==================== 字母表函数 ====================
struct char_set* get_alphabet(struct finite_automata* nfa);

// ==================== 移动操作函数 ====================
StateSet* move(struct finite_automata* nfa, StateSet* set, char c);

// ==================== NFA合并函数 ====================
struct finite_automata* combine_nfas(struct finite_automata** nfas, int num_nfas, int** accepting_states, int* num_accepting);

// ==================== DFA转换函数 ====================
struct finite_automata* nfa_to_dfa(struct finite_automata* nfa, int* accepting_states, int num_accepting, int* dfa_accepting_rules);

// ==================== 词法分析函数 ====================
void lexical_analysis(struct finite_automata* dfa, int* dfa_accepting_rules, char* input, int* segments, int* categories);

// ==================== 主流程函数 ====================
struct Lexer* generate_lexer(struct frontend_regexp** regexps, int num_regexps);
void run_lexer(struct Lexer* lexer, char* input);

// ==================== 内存释放函数 ====================
void free_lexer(struct Lexer* lexer);
void free_simpl_regexp(struct simpl_regexp* sr);
void free_frontend_regexp(struct frontend_regexp* fr);
void free_finite_automata(struct finite_automata* fa);

// ==================== 新增规则函数声明 ====================
struct frontend_regexp* create_operator_regex();
struct frontend_regexp* create_comparison_regex();
struct frontend_regexp* create_punctuation_regex();
struct frontend_regexp* create_bracket_regex();
struct frontend_regexp* create_symbol_regex();
void print_lexical_result(char* input, int* segments, int* categories);
struct frontend_regexp** create_default_rules(int* num_rules);

#endif // LEXER_H_INCLUDED