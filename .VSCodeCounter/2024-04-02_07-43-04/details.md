# Details

Date : 2024-04-02 07:43:04

Directory /home/lyj/os

Total : 71 files,  1317477 codes, 1303 comments, 843 blanks, all 1319623 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [README.md](/README.md) | Markdown | 2 | 0 | 1 | 3 |
| [bochs_out.log](/bochs_out.log) | Log | 1,312,893 | 0 | 1 | 1,312,894 |
| [makefile](/makefile) | Makefile | 63 | 10 | 12 | 85 |
| [src/boot/boot.inc](/src/boot/boot.inc) | Assembler file | 43 | 0 | 6 | 49 |
| [src/boot/loader.s](/src/boot/loader.s) | Assembler file | 319 | 0 | 55 | 374 |
| [src/boot/mbr.s](/src/boot/mbr.s) | Assembler file | 85 | 0 | 16 | 101 |
| [src/device/cmos.c](/src/device/cmos.c) | C | 89 | 15 | 25 | 129 |
| [src/device/cmos.h](/src/device/cmos.h) | C++ | 24 | 6 | 9 | 39 |
| [src/device/console.c](/src/device/console.c) | C | 35 | 7 | 8 | 50 |
| [src/device/console.h](/src/device/console.h) | C | 11 | 0 | 4 | 15 |
| [src/device/ide.c](/src/device/ide.c) | C | 249 | 89 | 30 | 368 |
| [src/device/ide.h](/src/device/ide.h) | C | 62 | 12 | 15 | 89 |
| [src/device/ioqueue.c](/src/device/ioqueue.c) | C | 52 | 12 | 14 | 78 |
| [src/device/ioqueue.h](/src/device/ioqueue.h) | C | 20 | 4 | 4 | 28 |
| [src/device/keyboard.c](/src/device/keyboard.c) | C | 174 | 29 | 22 | 225 |
| [src/device/keyboard.h](/src/device/keyboard.h) | C | 5 | 0 | 3 | 8 |
| [src/device/timer.c](/src/device/timer.c) | C | 53 | 32 | 13 | 98 |
| [src/device/timer.h](/src/device/timer.h) | C++ | 7 | 4 | 6 | 17 |
| [src/fs/dir.c](/src/fs/dir.c) | C | 132 | 94 | 21 | 247 |
| [src/fs/dir.h](/src/fs/dir.h) | C | 25 | 12 | 10 | 47 |
| [src/fs/file.c](/src/fs/file.c) | C | 299 | 157 | 49 | 505 |
| [src/fs/file.h](/src/fs/file.h) | C++ | 30 | 15 | 10 | 55 |
| [src/fs/fs.c](/src/fs/fs.c) | C | 294 | 162 | 41 | 497 |
| [src/fs/fs.h](/src/fs/fs.h) | C++ | 31 | 13 | 9 | 53 |
| [src/fs/inode.c](/src/fs/inode.c) | C | 109 | 87 | 19 | 215 |
| [src/fs/inode.h](/src/fs/inode.h) | C | 23 | 11 | 5 | 39 |
| [src/fs/super_block.c](/src/fs/super_block.c) | C | 1 | 10 | 0 | 11 |
| [src/fs/super_block.h](/src/fs/super_block.h) | C | 21 | 1 | 9 | 31 |
| [src/kernel/global.h](/src/kernel/global.h) | C | 59 | 35 | 17 | 111 |
| [src/kernel/init.c](/src/kernel/init.c) | C | 25 | 12 | 1 | 38 |
| [src/kernel/init.h](/src/kernel/init.h) | C | 4 | 2 | 2 | 8 |
| [src/kernel/interrupt.c](/src/kernel/interrupt.c) | C | 132 | 20 | 24 | 176 |
| [src/kernel/interrupt.h](/src/kernel/interrupt.h) | C | 29 | 9 | 10 | 48 |
| [src/kernel/kernel.s](/src/kernel/kernel.s) | Assembler file | 108 | 0 | 10 | 118 |
| [src/kernel/main.c](/src/kernel/main.c) | C | 40 | 14 | 12 | 66 |
| [src/kernel/memory.c](/src/kernel/memory.c) | C | 393 | 77 | 69 | 539 |
| [src/kernel/memory.h](/src/kernel/memory.h) | C | 50 | 25 | 14 | 89 |
| [src/lib/bitmap.c](/src/lib/bitmap.c) | C | 59 | 5 | 15 | 79 |
| [src/lib/bitmap.h](/src/lib/bitmap.h) | C | 13 | 5 | 6 | 24 |
| [src/lib/kernel/assert.c](/src/lib/kernel/assert.c) | C | 13 | 2 | 2 | 17 |
| [src/lib/kernel/assert.h](/src/lib/kernel/assert.h) | C | 10 | 8 | 4 | 22 |
| [src/lib/kernel/io.h](/src/lib/kernel/io.h) | C | 18 | 19 | 6 | 43 |
| [src/lib/kernel/print.c](/src/lib/kernel/print.c) | C | 60 | 5 | 7 | 72 |
| [src/lib/kernel/print.h](/src/lib/kernel/print.h) | C | 9 | 5 | 5 | 19 |
| [src/lib/kernel/put_char.s](/src/lib/kernel/put_char.s) | Assembler file | 120 | 0 | 12 | 132 |
| [src/lib/list.c](/src/lib/list.c) | C | 62 | 5 | 12 | 79 |
| [src/lib/list.h](/src/lib/list.h) | C | 26 | 14 | 8 | 48 |
| [src/lib/math.c](/src/lib/math.c) | C | 68 | 36 | 16 | 120 |
| [src/lib/math.h](/src/lib/math.h) | C | 36 | 0 | 7 | 43 |
| [src/lib/stdin.h](/src/lib/stdin.h) | C | 17 | 1 | 5 | 23 |
| [src/lib/stdio.c](/src/lib/stdio.c) | C | 117 | 6 | 14 | 137 |
| [src/lib/stdio.h](/src/lib/stdio.h) | C | 9 | 4 | 5 | 18 |
| [src/lib/string.c](/src/lib/string.c) | C | 111 | 14 | 11 | 136 |
| [src/lib/string.h](/src/lib/string.h) | C | 14 | 11 | 3 | 28 |
| [src/lib/user/syscall.c](/src/lib/user/syscall.c) | C | 53 | 8 | 9 | 70 |
| [src/lib/user/syscall.h](/src/lib/user/syscall.h) | C | 14 | 4 | 5 | 23 |
| [src/thread/mlfq.c](/src/thread/mlfq.c) | C | 143 | 41 | 13 | 197 |
| [src/thread/mlfq.h](/src/thread/mlfq.h) | C++ | 14 | 10 | 4 | 28 |
| [src/thread/switch.s](/src/thread/switch.s) | Assembler file | 23 | 0 | 2 | 25 |
| [src/thread/sync.c](/src/thread/sync.c) | C | 54 | 12 | 16 | 82 |
| [src/thread/sync.h](/src/thread/sync.h) | C | 21 | 8 | 5 | 34 |
| [src/thread/thread.c](/src/thread/thread.c) | C | 127 | 38 | 21 | 186 |
| [src/thread/thread.h](/src/thread/thread.h) | C | 73 | 31 | 12 | 116 |
| [src/userprog/process.c](/src/userprog/process.c) | C | 78 | 23 | 9 | 110 |
| [src/userprog/process.h](/src/userprog/process.h) | C | 13 | 0 | 4 | 17 |
| [src/userprog/syscall_init.c](/src/userprog/syscall_init.c) | C | 27 | 5 | 6 | 38 |
| [src/userprog/syscall_init.h](/src/userprog/syscall_init.h) | C++ | 7 | 0 | 3 | 10 |
| [src/userprog/tss.c](/src/userprog/tss.c) | C | 35 | 6 | 7 | 48 |
| [src/userprog/tss.h](/src/userprog/tss.h) | C++ | 35 | 1 | 7 | 43 |
| [tool/readme.md](/tool/readme.md) | Markdown | 6 | 0 | 5 | 11 |
| [tool/xxd.sh](/tool/xxd.sh) | Shell Script | 1 | 0 | 1 | 2 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)