Find missing headers
--------------------
```
cc -c -ansi -Wall -Wextra -pedantic filename.c 2>&1 | grep -E -o '<[^<>]+>' \
    | sort --uniq | sed -E 's/^/#include /'
```
