my_lexer.exe: main.c lexer.c lang_functions.c lang.h lexer.h
	gcc -o my_lexer.exe main.c lexer.c lang_functions.c

clean:
	del my_lexer.exe