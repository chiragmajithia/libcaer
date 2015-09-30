gcc -std=c99 -pedantic -Wall -Wextra -O2 -ggdb -o dvs128_simple dvs128_simple.c -D_POSIX_C_SOURCE=1 -D_BSD_SOURCE=1 -I /usr/include/libcaer/ -lcaer
