# CH32x35 Debugger - Debugger Tool for Port11 MSPM0 dev board

This tool is a work in progress.

## Tested On

This tool should work on MSPM0G3507 chips. But I haven't tested it on any other chips.

- [x] MSPM0G3507

## How to Generate Hex image

**Pre Requisites**

- ARM-CGT_CLANG Compiler

[Download ARM-CGT-CLANG Compiler](https://www.ti.com/tool/download/ARM-CGT-CLANG/3.2.2.LTS).

Note: Download Latest version

**Build**

Build your MSPM0G3507 program using either GCC or Code Composer studio (CSS).

**Generate Hex Image**

```
pathToClangCompiler/bin/tiarmhex --diag_wrap=off --intel --memwidth 8 --romwidth 8 -o [.hex application_image] [.out application_image]
```

For example:

```
pathToClangCompiler/bin/tiarmhex  --diag_wrap=off --intel --memwidth 8 --romwidth 8 -o application_image.hex application_image.out
```

## Author

[Goutham S Krishna](https://www.linkedin.com/in/goutham-s-krishna-21ab151a0/)
