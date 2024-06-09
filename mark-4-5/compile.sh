#! /bin/sh

gcc ./net/server.c ./net/server-tools.c ./util/parser.c -O2 -lrt -lpthread -o server
gcc ./net/first-worker.c ./net/client-tools.c ./util/parser.c -O2 -lrt -lm -o first-worker
gcc ./net/second-worker.c ./net/client-tools.c ./util/parser.c -O2 -lrt -lm -o second-worker
gcc ./net/third-worker.c ./net/client-tools.c ./util/parser.c -O2 -lrt -lm -o third-worker
