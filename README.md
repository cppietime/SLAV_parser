Datamatum and SLAV Parser, by Yaakov Schecmtan 2019.

This is an exercise project developed to provide support for some basic datatypes and parsing.
Including "slav.h" will include all necessary headers and functions for use.

See datatypes/datam.h for detailed comments and function signatures for the data types provided.
datam_darr is a dynamic array list.
datam_deque is a double-link-list based double-ended queue.
datam_unihash is a universal hash function parameter holding struct.
datam_hashtable is an open-addressed hash table.

Call regam_nfa_start\(\) before using any parsing functions to set up the necessary variables for use. Call regam_nfa_end\(\) once done.
slav_updog\(FILE \*src, slav_lang \*dst\) reads the file src in human-readable specified language and produces the parse table and language for it.
slav_ligma\(FILE \*src, slav_lang \*dst\) reads the already constructed and saved language from src into dst;
slav_sugma\(FILE \*dst, slav_lang \*src\) saves the language src to file dst;
slav_squat\(slav_lang \*lang\) deletes and cleans up lang. You still must call regam_nfa_end eventually.
slav_joe\(FILE \*src, slav_lang \*lang\) parses the contents of src using lang and returns a new parsam_ast representing the contents.

parsam_ast_delete\(parsam_ast \*ast) deletes and cleans up ast.
parsam_ast_print\(parsam_ast \*ast, parsam_table \*table, FILE \*file) prints the tree representation of ast to file. Use slav_lang.table for table.
parsam_table_print\(parsam_table \*table, FILE \*file) prints the representation of table to file.
regam_regex_print\(regam_regex \*regex) prints regex to stdout.

The human-readable language specification is as follows:

\[LIST OF TERMINALS SEPARATED BY SPACES. MUST END WITH THE ENDING TERMINAL\]\\n
\[LIST OF NON-TERMINALS SEPARATED BY SPACES. MUST END WITH THE STARTING NON-TEMRINAL\]\\n
\[NUMBER OF PRODUCTION RULES\]\\n
\[ONE PRODUCTION RULE PER LINE, OF THE FORM:\]\\n
\[TARGET NON-TEMRINAL\]:\[SPACE-SEPARATED LIST OF IN-ORDER SYMBOLS\]\\n
\[REGEXES TO SPECIFY TERMINAL SYMBOLS, FORMAT AS FOLLOWS, ONE PER LINE:\]\\n
\[TARGET TERMINAL\]:\[CORRESPONDING REGEX\]\\n
\[A TERMINAL NAMED "IGNORE" (CAPITALS) IS IGNORED BY THE LEXER\]\\n
OVER\\n
\[TERMINAL TOKEN REPLACEMENTS, ONE PER LINE. I.E. WHEN A TOKEN OF ONE TYPE HAS A PARTICULAR LEXEME, MAKE IT ANOTHER TYPE\]\\n
\[THESE USE THE FOLLOWING FORM:\]\\
\[TOKEN TO REPLACE\] \[TARGET TOKEN TO BE CHANGED TO\] \[TARGET LEXEME, LITERAL\]

Here is an example language specification:

id open close end
stmlist stmt start
5
start:stmlist end
stmlist:stmlist stmt
stmlist:stmt
stmt:open stmlist close
stmt:id
id:\\w+
open:\<
close:\>
IGNORE:\\s+
OVER
id open OPEN
id close CLOSE

See test.c for an example program. Compile with `make test`.