#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 字符集匹配检查
int char_in_set(char c, struct char_set* cs) {
    if (!cs || cs->n == 0) return 0;
    for (unsigned int i = 0; i < cs->n; i++) {
        if (cs->c[i] == c) return 1;
    }
    return 0;
}

// 创建字符集
struct char_set* create_char_set_from_range(char start, char end) {
    struct char_set* cs = malloc(sizeof(struct char_set));
    cs->n = end - start + 1;
    cs->c = malloc(cs->n);
    for (int i = 0; i < cs->n; i++) {
        cs->c[i] = start + i;
    }
    return cs;
}

struct char_set* create_char_set_from_chars(char* chars, int n) {
    struct char_set* cs = malloc(sizeof(struct char_set));
    cs->n = n;
    cs->c = malloc(n);
    memcpy(cs->c, chars, n);
    return cs;
}

// 状态集合管理
StateSet* create_state_set(int* states, int size, int id) {
    StateSet* set = malloc(sizeof(StateSet));
    set->states = malloc(size * sizeof(int));
    memcpy(set->states, states, size * sizeof(int));
    set->size = size;
    set->id = id;
    return set;
}

void free_state_set(StateSet* set) {
    if (set) {
        free(set->states);
        free(set);
    }
}

int state_set_equal(StateSet* a, StateSet* b) {
    if (a->size != b->size) return 0;
    for (int i = 0; i < a->size; i++) {
        if (a->states[i] != b->states[i]) return 0;
    }
    return 1;
}

void sort_state_set(StateSet* set) {
    for (int i = 0; i < set->size - 1; i++) {
        for (int j = i + 1; j < set->size; j++) {
            if (set->states[i] > set->states[j]) {
                int temp = set->states[i];
                set->states[i] = set->states[j];
                set->states[j] = temp;
            }
        }
    }
}

// 简化正则表达式
struct simpl_regexp* simplify_regexp(struct frontend_regexp* fr) {
    if (!fr) return NULL;
    
    switch (fr->t) {
        case T_FR_CHAR_SET: {
            struct char_set* cs = malloc(sizeof(struct char_set));
            copy_char_set(cs, &fr->d.CHAR_SET);
            return TS_CharSet(cs);
        }
        
        case T_FR_SINGLE_CHAR: {
            struct char_set* cs = malloc(sizeof(struct char_set));
            cs->n = 1;
            cs->c = malloc(1);
            cs->c[0] = fr->d.SINGLE_CHAR.c;
            return TS_CharSet(cs);
        }
        
        case T_FR_STRING: {
            char* s = fr->d.STRING.s;
            int len = strlen(s);
            if (len == 0) return TS_EmptyStr();
            
            struct simpl_regexp* result = NULL;
            for (int i = 0; i < len; i++) {
                struct char_set* cs = malloc(sizeof(struct char_set));
                cs->n = 1;
                cs->c = malloc(1);
                cs->c[0] = s[i];
                
                struct simpl_regexp* char_regexp = TS_CharSet(cs);
                if (result == NULL) {
                    result = char_regexp;
                } else {
                    result = TS_Concat(result, char_regexp);
                }
            }
            return result;
        }
        
        case T_FR_OPTIONAL: {
            struct simpl_regexp* r = simplify_regexp(fr->d.OPTION.r);
            return TS_Union(r, TS_EmptyStr());
        }
        
        case T_FR_STAR: {
            struct simpl_regexp* r = simplify_regexp(fr->d.STAR.r);
            return TS_Star(r);
        }
        
        case T_FR_PLUS: {
            struct simpl_regexp* r = simplify_regexp(fr->d.PLUS.r);
            return TS_Concat(r, TS_Star(r));
        }
        
        case T_FR_UNION: {
            struct simpl_regexp* r1 = simplify_regexp(fr->d.UNION.r1);
            struct simpl_regexp* r2 = simplify_regexp(fr->d.UNION.r2);
            return TS_Union(r1, r2);
        }
        
        case T_FR_CONCAT: {
            struct simpl_regexp* r1 = simplify_regexp(fr->d.CONCAT.r1);
            struct simpl_regexp* r2 = simplify_regexp(fr->d.CONCAT.r2);
            return TS_Concat(r1, r2);
        }
        
        default:
            return NULL;
    }
}

// NFA构建
NFAFragment regexp_to_nfa_fragment(struct finite_automata* nfa, struct simpl_regexp* sr) {
    NFAFragment frag = {-1, -1};
    if (!sr || !nfa) return frag;
    
    switch (sr->t) {
        case T_S_EMPTY_STR: {
            frag.start = add_one_vertex(nfa);
            frag.end = add_one_vertex(nfa);
            add_one_edge(nfa, frag.start, frag.end, NULL);
            break;
        }
        
        case T_S_CHAR_SET: {
            frag.start = add_one_vertex(nfa);
            frag.end = add_one_vertex(nfa);
            add_one_edge(nfa, frag.start, frag.end, &sr->d.CHAR_SET);
            break;
        }
        
        case T_S_STAR: {
            NFAFragment inner = regexp_to_nfa_fragment(nfa, sr->d.STAR.r);
            frag.start = add_one_vertex(nfa);
            frag.end = add_one_vertex(nfa);
            add_one_edge(nfa, frag.start, inner.start, NULL);
            add_one_edge(nfa, inner.end, frag.end, NULL);
            add_one_edge(nfa, inner.end, inner.start, NULL);
            add_one_edge(nfa, frag.start, frag.end, NULL);
            break;
        }
        
        case T_S_UNION: {
            frag.start = add_one_vertex(nfa);
            frag.end = add_one_vertex(nfa);
            NFAFragment left = regexp_to_nfa_fragment(nfa, sr->d.UNION.r1);
            NFAFragment right = regexp_to_nfa_fragment(nfa, sr->d.UNION.r2);
            add_one_edge(nfa, frag.start, left.start, NULL);
            add_one_edge(nfa, frag.start, right.start, NULL);
            add_one_edge(nfa, left.end, frag.end, NULL);
            add_one_edge(nfa, right.end, frag.end, NULL);
            break;
        }
        
        case T_S_CONCAT: {
            NFAFragment left = regexp_to_nfa_fragment(nfa, sr->d.CONCAT.r1);
            NFAFragment right = regexp_to_nfa_fragment(nfa, sr->d.CONCAT.r2);
            add_one_edge(nfa, left.end, right.start, NULL);
            frag.start = left.start;
            frag.end = right.end;
            break;
        }
    }
    
    return frag;
}

struct finite_automata* build_nfa_from_regexp(struct simpl_regexp* sr) {
    if (!sr) return NULL;
    struct finite_automata* nfa = create_empty_graph();
    if (!nfa) return NULL;
    regexp_to_nfa_fragment(nfa, sr);
    return nfa;
}

// ε-闭包计算
void epsilon_closure(struct finite_automata* nfa, int state, int* visited, int* closure, int* closure_size) {
    if (visited[state]) return;
    visited[state] = 1;
    closure[(*closure_size)++] = state;
    
    for (int e = 0; e < nfa->m; e++) {
        if (nfa->src[e] == state && nfa->lb[e].n == 0) {
            epsilon_closure(nfa, nfa->dst[e], visited, closure, closure_size);
        }
    }
}

int* get_epsilon_closure(struct finite_automata* nfa, int state, int* size) {
    int* visited = calloc(nfa->n, sizeof(int));
    int* closure = malloc(nfa->n * sizeof(int));
    *size = 0;
    epsilon_closure(nfa, state, visited, closure, size);
    free(visited);
    return closure;
}

// 字母表
struct char_set* get_alphabet(struct finite_automata* nfa) {
    bool chars[256] = {false};
    for (int e = 0; e < nfa->m; e++) {
        struct char_set* lb = &nfa->lb[e];
        for (unsigned int i = 0; i < lb->n; i++) {
            chars[(unsigned char)lb->c[i]] = true;
        }
    }
    
    int count = 0;
    for (int i = 0; i < 256; i++) {
        if (chars[i]) count++;
    }
    
    struct char_set* alphabet = malloc(sizeof(struct char_set));
    alphabet->n = count;
    alphabet->c = malloc(count);
    
    int idx = 0;
    for (int i = 0; i < 256; i++) {
        if (chars[i]) alphabet->c[idx++] = (char)i;
    }
    
    return alphabet;
}

// 移动操作
StateSet* move(struct finite_automata* nfa, StateSet* set, char c) {
    bool* reached = calloc(nfa->n, sizeof(bool));
    int reach_count = 0;
    
    for (int i = 0; i < set->size; i++) {
        int state = set->states[i];
        for (int e = 0; e < nfa->m; e++) {
            if (nfa->src[e] == state && nfa->lb[e].n > 0 && char_in_set(c, &nfa->lb[e])) {
                int dst = nfa->dst[e];
                if (!reached[dst]) {
                    reached[dst] = true;
                    reach_count++;
                }
            }
        }
    }
    
    int* states = malloc(reach_count * sizeof(int));
    int idx = 0;
    for (int i = 0; i < nfa->n; i++) {
        if (reached[i]) states[idx++] = i;
    }
    
    free(reached);
    return create_state_set(states, reach_count, -1);
}

// NFA到DFA转换（完整实现）
struct finite_automata* nfa_to_dfa(struct finite_automata* nfa, int* accepting_states, int num_accepting, int* dfa_accepting_rules) {
    struct finite_automata* dfa = create_empty_graph();
    if (!dfa) return NULL;
    
    // 初始化接受状态数组
    for (int i = 0; i < 1000; i++) dfa_accepting_rules[i] = -1;
    
    // 起始状态ε-闭包
    int start_closure_size;
    int* start_states = get_epsilon_closure(nfa, 0, &start_closure_size);
    StateSet* start_set = create_state_set(start_states, start_closure_size, 0);
    sort_state_set(start_set);
    free(start_states);
    
    // 工作列表
    StateSet** worklist = malloc(100 * sizeof(StateSet*));
    StateSet** statesets = malloc(100 * sizeof(StateSet*));
    int worklist_count = 0, stateset_count = 0;
    
    // 添加起始状态
    worklist[worklist_count++] = start_set;
    statesets[stateset_count++] = start_set;
    start_set->id = add_one_vertex(dfa);
    
    // 字母表
    struct char_set* alphabet = get_alphabet(nfa);
    
    while (worklist_count > 0) {
        StateSet* current = worklist[--worklist_count];
        
        for (unsigned int ci = 0; ci < alphabet->n; ci++) {
            char c = alphabet->c[ci];
            StateSet* moved = move(nfa, current, c);
            if (moved->size == 0) {
                free_state_set(moved);
                continue;
            }
            
            // 计算ε-闭包
            bool* closure_visited = calloc(nfa->n, sizeof(bool));
            int* closure_states = malloc(nfa->n * sizeof(int));
            int closure_size = 0;
            
            for (int i = 0; i < moved->size; i++) {
                int* partial_closure;
                int partial_size;
                partial_closure = get_epsilon_closure(nfa, moved->states[i], &partial_size);
                for (int j = 0; j < partial_size; j++) {
                    if (!closure_visited[partial_closure[j]]) {
                        closure_visited[partial_closure[j]] = true;
                        closure_states[closure_size++] = partial_closure[j];
                    }
                }
                free(partial_closure);
            }
            
            free_state_set(moved);
            if (closure_size == 0) {
                free(closure_visited);
                free(closure_states);
                continue;
            }
            
            StateSet* new_set = create_state_set(closure_states, closure_size, -1);
            sort_state_set(new_set);
            free(closure_visited);
            free(closure_states);
            
            // 查找是否已存在
            int found = -1;
            for (int i = 0; i < stateset_count; i++) {
                if (state_set_equal(new_set, statesets[i])) {
                    found = statesets[i]->id;
                    free_state_set(new_set);
                    break;
                }
            }
            
            if (found == -1) {
                new_set->id = add_one_vertex(dfa);
                worklist[worklist_count++] = new_set;
                statesets[stateset_count++] = new_set;
                found = new_set->id;
            }
            
            // 添加DFA边
            struct char_set* single_char = malloc(sizeof(struct char_set));
            single_char->n = 1;
            single_char->c = malloc(1);
            single_char->c[0] = c;
            add_one_edge(dfa, current->id, found, single_char);
        }
    }
    
    // 标记接受状态
    for (int i = 0; i < stateset_count; i++) {
        StateSet* set = statesets[i];
        for (int j = 0; j < set->size; j++) {
            for (int k = 0; k < num_accepting; k++) {
                if (set->states[j] == accepting_states[k]) {
                    dfa_accepting_rules[set->id] = k;
                    break;
                }
            }
        }
    }
    
    // 清理
    free(alphabet->c);
    free(alphabet);
    for (int i = 0; i < stateset_count; i++) free_state_set(statesets[i]);
    free(statesets);
    free(worklist);
    
    return dfa;
}

// 合并NFA
struct finite_automata* combine_nfas(struct finite_automata** nfas, int num_nfas, int** accepting_states, int* num_accepting) {
    if (num_nfas == 0) return NULL;
    
    struct finite_automata* combined = create_empty_graph();
    *accepting_states = malloc(num_nfas * sizeof(int));
    *num_accepting = num_nfas;
    
    int new_start = add_one_vertex(combined);
    int state_offset = 1;
    
    for (int i = 0; i < num_nfas; i++) {
        add_one_edge(combined, new_start, state_offset, NULL);
        
        for (int v = 0; v < nfas[i]->n; v++) {
            add_one_vertex(combined);
        }
        
        for (int e = 0; e < nfas[i]->m; e++) {
            int src = nfas[i]->src[e] + state_offset;
            int dst = nfas[i]->dst[e] + state_offset;
            add_one_edge(combined, src, dst, &nfas[i]->lb[e]);
        }
        
        (*accepting_states)[i] = state_offset + nfas[i]->n - 1;
        state_offset += nfas[i]->n;
    }
    
    return combined;
}

struct frontend_regexp* create_digit_regex() {
    struct char_set* digit_set = create_char_set_from_range('0', '9');
    return TFr_CharSet(digit_set);
}

// Test case 2: Letter recognition
struct frontend_regexp* create_alpha_regex() {
    struct char_set* lower_set = create_char_set_from_range('a', 'z');
    struct char_set* upper_set = create_char_set_from_range('A', 'Z');
    
    // Combine upper and lower case letters
    char all_alpha[52];
    for (int i = 0; i < 26; i++) {
        all_alpha[i] = 'a' + i;
        all_alpha[i + 26] = 'A' + i;
    }
    
    struct char_set* alpha_set = create_char_set_from_chars(all_alpha, 52);
    return TFr_CharSet(alpha_set);
}

// Test case 3: Identifier (letter followed by letters or digits)
struct frontend_regexp* create_identifier_regex() {
    // Letter
    struct char_set* alpha_set = create_char_set_from_range('a', 'z');
    struct frontend_regexp* alpha = TFr_CharSet(alpha_set);
    
    // Digit
    struct char_set* digit_set = create_char_set_from_range('0', '9');
    struct frontend_regexp* digit = TFr_CharSet(digit_set);
    
    // Letter or digit
    struct frontend_regexp* alpha_digit = TFr_Union(alpha, digit);
    
    // Identifier: letter (letter|digit)*
    struct frontend_regexp* identifier = TFr_Concat(alpha, TFr_Star(alpha_digit));
    return identifier;
}

// Test case 4: Integer (one or more digits)
struct frontend_regexp* create_integer_regex() {
    struct char_set* digit_set = create_char_set_from_range('0', '9');
    struct frontend_regexp* digit = TFr_CharSet(digit_set);
    return TFr_Plus(digit);
}

// Test case 5: Whitespace characters
struct frontend_regexp* create_whitespace_regex() {
    char whitespace_chars[] = {' ', '\t', '\n', '\r'};
    struct char_set* ws_set = create_char_set_from_chars(whitespace_chars, 4);
    return TFr_Plus(TFr_CharSet(ws_set));
}

// 新增的规则创建函数
struct frontend_regexp* create_operator_regex() {
    char operators[] = {'=', '+', '-', '*', '/', '%', '!', '&', '|', '^', '~'};
    struct char_set* op_set = create_char_set_from_chars(operators, 11);
    return TFr_CharSet(op_set);
}

struct frontend_regexp* create_comparison_regex() {
    char comparisons[] = {'<', '>', '='};
    struct char_set* comp_set = create_char_set_from_chars(comparisons, 3);
    return TFr_CharSet(comp_set);
}

struct frontend_regexp* create_punctuation_regex() {
    char punctuation[] = {',', ';', ':', '.', '?', '!', '"', '\''};
    struct char_set* punct_set = create_char_set_from_chars(punctuation, 8);
    return TFr_CharSet(punct_set);
}

struct frontend_regexp* create_bracket_regex() {
    char brackets[] = {'(', ')', '[', ']', '{', '}'};
    struct char_set* bracket_set = create_char_set_from_chars(brackets, 6);
    return TFr_CharSet(bracket_set);
}

struct frontend_regexp* create_symbol_regex() {
    char symbols[] = {'@', '#', '$', '_', '\\'};
    struct char_set* symbol_set = create_char_set_from_chars(symbols, 5);
    return TFr_CharSet(symbol_set);
}


// 词法分析
void lexical_analysis(struct finite_automata* dfa, int* dfa_accepting_rules, char* input, int* segments, int* categories) {
    int pos = 0, input_len = strlen(input), segment_count = 0;
    int current_state = 0, last_accepting_state = -1, last_accepting_pos = -1, start_pos = 0;
    
    while (pos <= input_len) {
        if (current_state != -1 && dfa_accepting_rules[current_state] != -1) {
            last_accepting_state = current_state;
            last_accepting_pos = pos;
        }
        
        if (pos < input_len) {
            int next_state = -1;
            char current_char = input[pos];
            
            for (int e = 0; e < dfa->m; e++) {
                if (dfa->src[e] == current_state && dfa->lb[e].n > 0 && char_in_set(current_char, &dfa->lb[e])) {
                    next_state = dfa->dst[e];
                    break;
                }
            }
            
            if (next_state != -1) {
                current_state = next_state;
                pos++;
            } else {
                if (last_accepting_state != -1) {
                    segments[segment_count] = start_pos;
                    categories[segment_count] = dfa_accepting_rules[last_accepting_state];
                    segment_count++;
                    start_pos = last_accepting_pos;
                    pos = last_accepting_pos;
                    current_state = 0;
                    last_accepting_state = -1;
                } else {
                    segments[segment_count] = start_pos;
                    categories[segment_count] = -1;
                    segment_count++;
                    start_pos = pos + 1;
                    pos++;
                    current_state = 0;
                }
            }
        } else {
            if (last_accepting_state != -1) {
                segments[segment_count] = start_pos;
                categories[segment_count] = dfa_accepting_rules[last_accepting_state];
                segment_count++;
            }
            break;
        }
    }
    
    segments[segment_count] = -1;
    categories[segment_count] = -1;
}

// 打印词法分析结果
void print_lexical_result(char* input, int* segments, int* categories) {
    const char* rule_names[] = {
        "WHITESPACE", "IDENTIFIER", "INTEGER", "OPERATOR", "COMPARISON", 
        "BRACKET", "PUNCTUATION", "SYMBOL", "ALPHA", "DIGIT"
    };
    
    printf("Input string: \"%s\"\n", input);
    printf("Lexical analysis results:\n");
    printf("Pos\tLen\tType\t\tContent\n");
    printf("------------------------------------------------\n");
    
    for (int i = 0; segments[i] != -1; i++) {
        int start = segments[i];
        int end = (segments[i + 1] != -1) ? segments[i + 1] : strlen(input);
        int length = end - start;
        
        if (length > 0) {
            char* content = malloc(length + 1);
            strncpy(content, input + start, length);
            content[length] = '\0';
            
            const char* type_name = (categories[i] >= 0 && categories[i] < 10) ? 
                                   rule_names[categories[i]] : "UNKNOWN";
            
            printf("%d\t%d\t%s\t\t\"%s\"\n", start, length, type_name, content);
            free(content);
        }
    }
    printf("\n");
}

// 创建默认规则数组
struct frontend_regexp** create_default_rules(int* num_rules) {
    *num_rules = 10;
    struct frontend_regexp** regexps = malloc(10 * sizeof(struct frontend_regexp*));
    
    regexps[0] = create_whitespace_regex();    // Rule 0: whitespace
    regexps[1] = create_identifier_regex();    // Rule 1: identifier
    regexps[2] = create_integer_regex();       // Rule 2: integer
    regexps[3] = create_operator_regex();      // Rule 3: operator
    regexps[4] = create_comparison_regex();    // Rule 4: comparison
    regexps[5] = create_bracket_regex();       // Rule 5: bracket
    regexps[6] = create_punctuation_regex();   // Rule 6: punctuation
    regexps[7] = create_symbol_regex();        // Rule 7: symbol
    regexps[8] = create_alpha_regex();         // Rule 8: single letter
    regexps[9] = create_digit_regex();         // Rule 9: single digit
    
    return regexps;
}


// 修复后的 generate_lexer 函数 - 关键修复！
struct Lexer* generate_lexer(struct frontend_regexp** regexps, int num_regexps) {
    // 直接使用传入的规则，不要额外添加
    // 简化正则表达式
    struct simpl_regexp** simplified = malloc(num_regexps * sizeof(struct simpl_regexp*));
    for (int i = 0; i < num_regexps; i++) {
        simplified[i] = simplify_regexp(regexps[i]);
    }
    
    // 构建NFA
    struct finite_automata** nfas = malloc(num_regexps * sizeof(struct finite_automata*));
    for (int i = 0; i < num_regexps; i++) {
        nfas[i] = build_nfa_from_regexp(simplified[i]);
    }
    
    // 合并NFA并转换为DFA
    int* nfa_accepting_states;
    int num_accepting;
    struct finite_automata* combined_nfa = combine_nfas(nfas, num_regexps, &nfa_accepting_states, &num_accepting);
    
    int* dfa_accepting_rules = malloc(1000 * sizeof(int));
    struct finite_automata* dfa = nfa_to_dfa(combined_nfa, nfa_accepting_states, num_accepting, dfa_accepting_rules);
    
    struct Lexer* lexer = malloc(sizeof(struct Lexer));
    lexer->dfa = dfa;
    lexer->dfa_accepting_rules = dfa_accepting_rules;
    lexer->dfa_size = dfa->n;
    
    // 清理临时内存
    for (int i = 0; i < num_regexps; i++) {
        // 需要实现相应的free函数
    }
    free(simplified);
    free(nfas);
    free(nfa_accepting_states);
    
    return lexer;
}

void run_lexer(struct Lexer* lexer, char* input) {
    int segments[1000];
    int categories[1000];
    lexical_analysis(lexer->dfa, lexer->dfa_accepting_rules, input, segments, categories);
    print_lexical_result(input, segments, categories);
}
