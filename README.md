Datamatum and SLAV Parser, by Yaakov Schecmtan 2019.<br/>

This is an exercise project developed to provide support for some basic datatypes and parsing.<br/>
Including "slav.h" will include all necessary headers and functions for use.<br/>

See datatypes/datam.h for detailed comments and function signatures for the data types provided.<br/>
`datam_darr` is a dynamic array list.<br/>
`datam_deque` is a double-link-list based double-ended queue.<br/>
`datam_unihash` is a universal hash function parameter holding struct.<br/>
`datam_hashtable` is an open-addressed hash table.<br/>

Call `regam_nfa_start\(\)` before using any parsing functions to set up the necessary variables for use. Call `regam_nfa_end\(\)` once done.<br/>
`slav_updog\(FILE \*src, slav_lang \*dst\)` reads the file src in human-readable specified language and produces the parse table and language for it.<br/>
`slav_ligma\(FILE \*src, slav_lang \*dst\)` reads the already constructed and saved language from src into dst;<br/>
`slav_sugma\(FILE \*dst, slav_lang \*src\)` saves the language src to file dst;<br/>
`slav_squat\(slav_lang \*lang\)` deletes and cleans up lang. You still must call regam_nfa_end eventually.<br/>
`slav_joe\(FILE \*src, slav_lang \*lang\)` parses the contents of src using lang and returns a new parsam_ast representing the contents.<br/>

`parsam_ast_delete\(parsam_ast \*ast)` deletes and cleans up ast.<br/>
`parsam_ast_print\(parsam_ast \*ast, parsam_table \*table, FILE \*file)` prints the tree representation of ast to file. Use slav_lang.table for table.<br/>
`parsam_table_print\(parsam_table \*table, FILE \*file)` prints the representation of table to file.<br/>
`regam_regex_print\(regam_regex \*regex)` prints regex to stdout.<br/>

The human-readable language specification is as follows:<br/>

```
\[LIST OF TERMINALS SEPARATED BY SPACES. MUST END WITH THE ENDING TERMINAL\]\\n<br/>
\[LIST OF NON-TERMINALS SEPARATED BY SPACES. MUST END WITH THE STARTING NON-TEMRINAL\]\\n<br/>
\[NUMBER OF PRODUCTION RULES\]\\n<br/>
\[ONE PRODUCTION RULE PER LINE, OF THE FORM:\]\\n<br/>
\[TARGET NON-TEMRINAL\]:\[SPACE-SEPARATED LIST OF IN-ORDER SYMBOLS\]\\n<br/>
\[REGEXES TO SPECIFY TERMINAL SYMBOLS, FORMAT AS FOLLOWS, ONE PER LINE:\]\\n<br/>
\[TARGET TERMINAL\]:\[CORRESPONDING REGEX\]\\n<br/>
\[A TERMINAL NAMED "IGNORE" (CAPITALS) IS IGNORED BY THE LEXER\]\\n<br/>
OVER\\n<br/>
\[TERMINAL TOKEN REPLACEMENTS, ONE PER LINE. I.E. WHEN A TOKEN OF ONE TYPE HAS A PARTICULAR LEXEME, MAKE IT ANOTHER TYPE\]\\n<br/>
\[THESE USE THE FOLLOWING FORM:\]\\n<br/>
\[TOKEN TO REPLACE\] \[TARGET TOKEN TO BE CHANGED TO\] \[TARGET LEXEME, LITERAL\]<br/>
```

Here is an example language specification:<br/>

```
id open close end<br/>
stmlist stmt start<br/>
5<br/>
start:stmlist end<br/>
stmlist:stmlist stmt<br/>
stmlist:stmt<br/>
stmt:open stmlist close<br/>
stmt:id<br/>
id:\\w+<br/>
open:\<<br/>
close:\><br/>
IGNORE:\\s+<br/>
OVER<br/>
id open OPEN<br/>
id close CLOSE<br/>
```

See test.c for an example program. Compile with `make test`.<br/>