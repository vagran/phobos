# PhobOS
Operating system written in C++ for educational purposes when I was a student.

`mainline` and `atomic` are actually branches (imported from SVN). Mainline was initial attempt which ended up with minimalistic more or less traditional OS with functional user space. Its key feature was transparent transition from user space to kernel mode just by calling virtual methods of specially crafted classes. This was achieved by some hacking and exploiting compiler specifics for compiled C++ code (looks ugly for me now, but these days I wanted some cool way for making syscalls without traditional syscalls declaration). `atomic` branch actually is full rewrite for new idea to make orthogonally persistent OS. It did not achieve same completion state as mainline but there are some good example, e.g. fully completed EFI loader implementation.

Both projects do not pretend to be used or even compiled nowadays, but might have historical value and have some useful code snippets.
