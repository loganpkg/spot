suco
====

Suco, named after a nickname that I use for my son, is a text editor
written from scratch.

The project aims to make clean, modular, reusable, and fully refactored
code.

Features
--------

* Built-in terminal graphics with split screen.
* Only the fundamental commands of insert, delete, left and right character
  directly make changes to the buffer.
* Undo and redo.
* Easy to configure key mappings.
* Cross-platform, primarily ANSI C.

Key mappings
------------

The key mappings can be configured easily in the file `key_mappings.txt`.
You then need to [build](#Building) in order for the changes to take effect.

[Key mappings](.key_mappings_table.md)


Building
--------

On POSIX-like systems, simply run:
```
./build.sh
```
...  which automatically processes changes to the key mappings.


Otherwise, if you modified the `key_mappings.txt` file, run:
```
./process_key_mappings.sh
```

Then run:
```
.\wbuild.cmd
```


Enjoy,
Logan =)_
