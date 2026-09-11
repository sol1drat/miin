compiler  := "gcc"
flags     := "-Wall -Wextra -Werror"
binary    := "inmi"
source    := "main.c"
test_file := "test.inm"

@build:
    {{compiler}} {{flags}} -o {{binary}} {{source}}

@test *args: build
    ./{{binary}} {{test_file}} {{args}} 
