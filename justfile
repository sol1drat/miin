compiler  := "gcc"
flags     := "-Wall -Wextra -Werror"
binary    := "miin"
source    := "main.c"
test_file := "test.mii"

@build:
    {{compiler}} {{flags}} -o {{binary}} {{source}}

@test *args: build
    ./{{binary}} {{test_file}} {{args}} 

@lex: build
    ./{{binary}} {{test_file}} --lex

@ast: build
    ./{{binary}} {{test_file}} --ast

@install: build
    cp ./{{binary}} ~/.local/bin
