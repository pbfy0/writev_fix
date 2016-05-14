# writev_fix
Workaround for broken writev syscall on windows subsystem for linux

To use, download the `bin` folder to somewhere you can access it on WSL. To launch a program with emulated `writev`, use `bin/wf program args`. Example: `bin/wf ls /`
