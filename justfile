compiler  := "gcc"
flags     := "-Wall -Wextra -Werror"
binary    := "miin"
source    := "main.c"
test_file := "test.mii"

clean:
    rm ~/.local/bin/miin
    rm ./{{binary}}

@build:
    {{compiler}} {{flags}} -o {{binary}} {{source}}

@install: build
    cp ./{{binary}} ~/.local/bin

@test *args: install
    ./{{binary}} {{test_file}} {{args}} 

@lex: install
    ./{{binary}} {{test_file}} --lex

@ast: install
    ./{{binary}} {{test_file}} --ast
