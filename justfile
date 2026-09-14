set positional-arguments

compiler := "gcc"

source    := "main.c"
test_file := "test.mii"

binary_asan     := "miin-asan"
binary_valgrind := "miin-val"
binary_release  := "miin"

bindir := home_dir() + "/.local/bin"

common_flags := "-Wall -Wextra -g -fno-omit-frame-pointer"

asan_flags := common_flags + " -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined"

vg := "valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1"

export ASAN_OPTIONS  := "halt_on_error=1:detect_leaks=1:detect_stack_use_after_return=1:symbolize=1"
export UBSAN_OPTIONS := "halt_on_error=1:print_stacktrace=1"

use_color := if env_var_or_default("NO_COLOR", "") == "" { "1" } else { "0" }

red     := if use_color == "1" { '\033[1;31m' } else { '' }
green   := if use_color == "1" { '\033[1;32m' } else { '' }
yellow  := if use_color == "1" { '\033[1;33m' } else { '' }
blue    := if use_color == "1" { '\033[1;34m' } else { '' }
magenta := if use_color == "1" { '\033[1;35m' } else { '' }
cyan    := if use_color == "1" { '\033[1;36m' } else { '' }
reset   := if use_color == "1" { '\033[0m' }  else { '' }

# List available recipes
[private]
default:
    @just --list

# Compile the sanitizer build (ASan + UBSan)
[group('build')]
build-asan:
    @printf '{{cyan}}[ BUILD ]{{reset}} ASan + UBSan\n'
    {{compiler}} {{asan_flags}} -o "{{binary_asan}}" "{{source}}"
    @printf '{{green}}[ OK ]{{reset}} built {{binary_asan}}\n'

# Compile the plain -g build that Valgrind runs against
[group('build')]
build-valgrind:
    @printf '{{cyan}}[ BUILD ]{{reset}} plain (for Valgrind)\n'
    {{compiler}} {{common_flags}} -o "{{binary_valgrind}}" "{{source}}"
    @printf '{{green}}[ OK ]{{reset}} built {{binary_valgrind}}\n'

# Compile the release binary
[group('build')]
build-release:
    @printf '{{cyan}}[ BUILD ]{{reset}} release\n'
    {{compiler}} -O2 -DNDEBUG {{common_flags}} -o "{{binary_release}}" "{{source}}"
    @printf '{{green}}[ OK ]{{reset}} built {{binary_release}}\n'

# Compile all build variants
[group('build')]
build: build-asan build-valgrind build-release
    @printf '{{green}}[ OK ]{{reset}} all builds succeeded\n'

# Run the ASan/ build
[group('dev')]
run *args: build-asan
    @printf '{{magenta}}[ RUN ]{{reset}} ./{{binary_asan}} {{test_file}}\n'
    @./"{{binary_asan}}" "{{test_file}}" "$@" || { c=$?; printf '\n{{red}}[ FAIL ]{{reset}} sanitizer run exited %d\n' "$c"; exit "$c"; }

# Interactive gdb session on the plain build
[group('dev')]
debug *args: build-valgrind
    @printf '{{yellow}}[ GDB ]{{reset}} ./{{binary_valgrind}} {{test_file}}\n'
    @gdb -q --args ./"{{binary_valgrind}}" "{{test_file}}" "$@"

# Full check
[group('test')]
check *args: build-asan build-valgrind
    @printf '{{magenta}}[ ASAN ]{{reset}} ./{{binary_asan}} {{test_file}}\n'
    @./"{{binary_asan}}" "{{test_file}}" "$@" || { c=$?; printf '\n{{red}}[ FAIL ]{{reset}} ASan/UBSan exited %d, skipping Valgrind\n' "$c"; exit "$c"; }
    @printf '{{magenta}}[ VALGRIND ]{{reset}} ./{{binary_valgrind}} {{test_file}}\n'
    @{{vg}} ./"{{binary_valgrind}}" "{{test_file}}" "$@" || { c=$?; printf '\n{{red}}[ FAIL ]{{reset}} Valgrind exited %d\n' "$c"; exit "$c"; }
    @printf '\n{{green}}[ OK ]{{reset}} sanitizers and Valgrind successful\n'

# Valgrind only (no sanitizer build involved)
[group('test')]
valgrind *args: build-valgrind
    @printf '{{magenta}}[ VALGRIND ]{{reset}} ./{{binary_valgrind}} {{test_file}}\n'
    @{{vg}} ./"{{binary_valgrind}}" "{{test_file}}" "$@" || { c=$?; printf '\n{{red}}[ FAIL ]{{reset}} Valgrind exited %d\n' "$c"; exit "$c"; }

# Print the lexical analysis result
[group('test')]
lex: build-valgrind
    @printf '{{magenta}}[ LEX ]{{reset}} ./{{binary_valgrind}} {{test_file}} --lex\n'
    @{{vg}} ./"{{binary_valgrind}}" "{{test_file}}" --lex || { c=$?; printf '\n{{red}}[ FAIL ]{{reset}} lex run exited %d\n' "$c"; exit "$c"; }

# Print the AST
[group('test')]
ast: build-valgrind
    @printf '{{magenta}}[ AST ]{{reset}} ./{{binary_valgrind}} {{test_file}} --ast\n'
    @{{vg}} ./"{{binary_valgrind}}" "{{test_file}}" --ast || { c=$?; printf '\n{{red}}[ FAIL ]{{reset}} ast run exited %d\n' "$c"; exit "$c"; }

# Run GCC's static analyzer
[group('lint')]
analyze:
    @printf '{{cyan}}[ ANALYZE ]{{reset}} gcc -fanalyzer\n'
    gcc -fanalyzer {{common_flags}} -c -o /dev/null "{{source}}"
    @printf '{{green}}[ OK ]{{reset}} analyzer finished\n'

# Install the release binary to ~/.local/bin
[group('install')]
install: build-release
    @printf '{{blue}}[ INSTALL ]{{reset}} {{binary_release}} -> {{bindir}}/{{binary_release}}\n'
    mkdir -p "{{bindir}}"
    install -m 0755 "{{binary_release}}" "{{bindir}}/{{binary_release}}"
    @printf '{{green}}[ OK ]{{reset}} installed {{bindir}}/{{binary_release}}\n'

# Remove build artifacts and the installed binary
[group('cleanup')]
clean:
    @printf '{{red}}[ CLEAN ]{{reset}} removing binaries...\n'
    rm -f "{{binary_asan}}" "{{binary_valgrind}}" "{{binary_release}}" "{{bindir}}/{{binary_release}}"
    @printf '{{green}}[ OK ]{{reset}} clean\n'
