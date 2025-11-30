#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lang.h"
#include "lexer.h"
// 测试函数
void test_lexer() {
    printf("========== Compiler Principles Lexer Test ==========\n\n");
    
    int num_rules;
    struct frontend_regexp** regexps = create_default_rules(&num_rules);
    
    // Generate lexer
    printf("Generating lexer...\n");
    struct Lexer* lexer = generate_lexer(regexps, num_rules);
    printf("Lexer generation completed!\n\n");
    
    // Test cases
    char* test_cases[] = {
        "hello world 123",
        "var x = 42;",
        "if (a < 10) { return \"hello\"; }",
        "abc123 def456",
        "  multiple   spaces  ",
        "mixed123and456numbers",
        "error!@# tokens",
        "x = y + z * 2;",
        "array[5] = {1, 2, 3};",
        "if (score >= 90) { grade = 'A'; }",
        NULL
    };
    
    // Run tests
    for (int i = 0; test_cases[i] != NULL; i++) {
        printf("Test case %d:\n", i + 1);
        
        int segments[100];
        int categories[100];
        
        lexical_analysis(lexer->dfa, lexer->dfa_accepting_rules, test_cases[i], segments, categories);
        print_lexical_result(test_cases[i], segments, categories);
    }
    
    // Interactive testing
    printf("========== Interactive Testing ==========\n");
    printf("Type 'quit' to exit\n\n");
    
    char input[256];
    while (1) {
        printf("Enter string to analyze: ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "quit") == 0) break;
        
        int segments[100];
        int categories[100];
        
        lexical_analysis(lexer->dfa, lexer->dfa_accepting_rules, input, segments, categories);
        print_lexical_result(input, segments, categories);
    }
    
    printf("Testing completed!\n");
    free(regexps);
}

int main() {
   
    // Run functional tests
    test_lexer();
    
    return 0;
}