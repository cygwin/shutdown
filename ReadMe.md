Cygwin Shutdown
===============

These are the official sources of the [Cygwin](https://cygwin.com) `shutdown` command.

Since Cygwin is not a complete Linux kernel, some low-end stuff like shutting down the machine won't work the Linux way. Therefore Cygwin has its own shutdown program. It is a simple program that mimic the Linux [shutdown](http://linux.die.net/man/8/shutdown) command but uses the [Win32-API](https://msdn.microsoft.com/en-us/library/windows/desktop/aa376883%28v=vs.85%29.aspx) to work on Windows.

Report [issues](https://github.com/cygwin/shutdown/issues) here or on the [Cygwin mailing list](https://www.cygwin.com/ml/cygwin/).

[Pull requests](https://github.com/cygwin/shutdown/pulls) will always be reviewed or https://cygwin.com/acronyms/#PTC

The repo can also be found at https://sourceware.org/git/?p=cygwin-apps/shutdown.git

Version history
---------------

Version 2.0 (2016-12-15)

* Added `--install` to install Windows Updates during shutdown/reboot. The `InitiateShutdown()` Windows API call is used for this. This function is only available from Windows 6.0 (aka Vista or Server 2008) or higher. To assure backwards compatibility with WinXP and Server 2003, `InitiateShutdown()` is loaded dynamically at runtime. Since Cygwin 2.6 support for WinXP and Server 2003 has been dropped, so this backward compatibility is likely to be removed in a future release.
* Added `--hybrid` to shutdown in hybrid mode. Hybrid is the default shutdown method with shutting down with the UI of Windows 8.x and higher.
* Changed the default shutdown messages.
* User can supply his own shutdown message on the command line.


Version 1.10 (2013-06-03)

* Changed the short option for hibernate from `-h` to `-b`.
* Added `-h`, `--halt` as additional option for shutdown and `-c`, `--cancel` as additional option for abort.
* Updated `shutdown.8` man pages to reflect these changes.
* Fixed typo: changed text InitateSystemShutdown to InitiateSystemShutdownEx in the two `usage()` functions in `shutdown.c` and in `shutdown.8` and `reboot.8`.


Version 1.9 (2013-03-21)

* The aliases now act like their Linux equivalents. They have their own command line parsing. It is no longer possible to do something like `hibernate --reboot 18:30`. And up till now the time was required for the aliases, just like the shutdown command. Now the aliases don't allow a time on the command line.
* Added aliases for `halt` and `poweroff`.
* The aliases have their own `--help` text.
* Added basic `shutdown.8` and `reboot.8` man pages.
