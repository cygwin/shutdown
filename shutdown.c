/*
 * shutdown.c: implementation of shutdown(1) as part of a Cygwin environment
 *
 * Copyright 1998, 2001, 2003, 2005, 2012, 2013  Corinna Vinschen,
 *           2013 Frank Fesevur
 * Bug reports to  cygwin@cygwin.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <time.h>

#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <reason.h>

/* The following values must not collide with EWX_* values. */
#define HIBERNATE	 32
#define SUSPEND		 64
#define ABORT		128

static char *SCCSid = "@(#)shutdown V1.10, Corinna Vinschen, " __DATE__ "\n";

#define MAXBUF 4096

char *myname;
long secs = -1;
int action = EWX_POWEROFF;
int force = 0;
BOOL force_exitex = FALSE;
BOOL install_updates = FALSE;
BOOL at_least_vista = FALSE;
BOOL hybrid_shutdown = FALSE;
char msgbuf[MAXBUF];
char timebuf[MAXBUF];
char errbuf[MAXBUF];

// For dynamically loading InitiateShutdown()
HMODULE hLibrary = NULL;
typedef DWORD (WINAPI *LPINITIATESHUTDOWN)(LPTSTR lpMachineName, LPTSTR lpMessage, DWORD dwGracePeriod, DWORD dwShutdownFlags, DWORD dwReason);

char *error(DWORD err)
{
	sprintf (errbuf, "Error %lu ", err);
	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR) errbuf + strlen (errbuf),
					sizeof (errbuf) - strlen (errbuf), NULL);
	return errbuf;
}

// Show the options valid for both the "shutdown" and the short alias variants
void usage_general(void)
{
	printf ("  -f, --force      Forces the execution.\n");
	printf ("  -i, --install    Install Windows Updates during shutdown or reboot.\n");
	printf ("  -H, --hybrid     Shutdown in hybrid mode and prepare it for fast startup.\n");
	printf ("  -c, --cancel     Aborts execution of formerly started shutdown.\n");
	printf ("  -a, --abort      Aborts execution of formerly started shutdown.\n");
	printf ("  -x, --exitex     Use ExitWindowsEx rather than InitiateSystemShutdownEx.\n");
	printf ("      --help       Display this help and exit.\n");
	printf ("      --version    Output version information and exit.\n");
}

// Show the usage for the "shutdown" command
int usage_shutdown(void)
{
	printf ("Usage: %s [option]... time [message]\n", myname);
	printf ("Bring the system down.\n\n");
	printf ("  -h, --halt       The system will shutdown and power off (if supported)\n");
	printf ("  -s, --shutdown   The system will shutdown and power off (if supported)\n");
	printf ("  -r, --reboot     The system will reboot.\n");
	printf ("  -b, --hibernate  The system will suspend to disk (if supported)\n");
	printf ("  -p, --suspend    The system will suspend to RAM (if supported)\n");
	usage_general();
	printf ("\n`time' is either the time in seconds or `+' and the time in minutes or a\n");
	printf ("timestamp in the format `hh:mm' or the word \"now\" for an immediate action.\n");
	return 0;
}

// Show the usage for the short alias commands
int usage_reboot(void)
{
	printf ("Usage: %s [option]...\n", myname);

	switch (action)
	{
		case EWX_POWEROFF:
			printf ("Bring the system down.\n\n");
			break;
		case EWX_REBOOT:
			printf ("Reboot the system.\n\n");
			break;
		case HIBERNATE:
			printf ("Suspend the system to disk.\n\n");
			break;
		case SUSPEND:
			printf ("Suspend the system to RAM.\n\n");
		break;
	}

	usage_general();
	return 0;
}

int version(void)
{
	printf ("%s\n", SCCSid + 4);
	printf ("Copyright (C) 2005-2013 Corinna Vinschen, Frank Fesevur\n");
	printf ("This is free software; see the source for copying conditions.\n");
	printf ("There is NO warranty; not even for MERCHANTABILITY or FITNESS\n");
	printf ("FOR A PARTICULAR PURPOSE.\n");
	return 0;
}

int setprivs (void)
{
	HANDLE token;
	TOKEN_PRIVILEGES privs;

	/* If the privilege hasn't been found, we're trying to shutdown anyway. */
	if (!LookupPrivilegeValue (NULL, SE_SHUTDOWN_NAME, &privs.Privileges[0].Luid))
	{
		fprintf (stderr, "%s: Warning: can't evaluate privilege: %s\n", myname, error (GetLastError ()));
		return 0;
	}

	privs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	privs.PrivilegeCount = 1;

	if (!OpenProcessToken (GetCurrentProcess (), TOKEN_ADJUST_PRIVILEGES, &token))
	{
		fprintf (stderr, "%s: can't open process token: %s\n", myname, error (GetLastError ()));
		return 1;
	}
	if (!AdjustTokenPrivileges (token, FALSE, &privs, 0, NULL, NULL))
	{
		fprintf (stderr, "%s: can't set required privilege: %s\n", myname, error (GetLastError ()));
		return 1;
	}
	if (GetLastError () == ERROR_NOT_ALL_ASSIGNED)
	{
		fprintf (stderr, "%s: required privilege not held: %s\n", myname, error (GetLastError ()));
		return 1;
	}
	if (!RevertToSelf ())
	{
		fprintf (stderr, "%s: can't activate required privilege: %s\n", myname, error (GetLastError ()));
		return 1;
	}
	return 0;
}

/* parse command line for the shutdown command */
int parse_cmdline_shutdown(int argc, char **argv)
{
	struct option longopts[] = {
		{"abort", no_argument, NULL, 'a'},
		{"cancel", no_argument, NULL, 'c'},
		{"exitex", no_argument, NULL, 'x'},
		{"force", no_argument, NULL, 'f'},
		{"shutdown", no_argument, NULL, 's'},
		{"halt", no_argument, NULL, 'h'},
		{"reboot", no_argument, NULL, 'r'},
		{"hibernate", no_argument, NULL, 'b'},
		{"suspend", no_argument, NULL, 'p'},
		{"install", no_argument, NULL, 'i'},
		{"hybrid", no_argument, NULL, 'H'},
		{"help", no_argument, NULL, 'E'},
		{"version", no_argument, NULL, 'v'},
		{0, no_argument, NULL, 0}
	};

	char opts[] = "acxfshrbpi";
	int c;
	char *arg, *endptr;

	while ((c = getopt_long (argc, argv, opts, longopts, NULL)) != EOF)
	{
		switch (c)
		{
			case 'f':
				force = EWX_FORCE;
				break;
			case 's':
			case 'h':
				action = EWX_POWEROFF;
				break;
			case 'r':
				action = EWX_REBOOT;
				break;
			case 'b':
				action = HIBERNATE;
				break;
			case 'p':
				action = SUSPEND;
				break;
			case 'i':
				install_updates = TRUE;
				break;
			case 'a':
			case 'c':
				action = ABORT;
				break;
			case 'x':
				force_exitex = TRUE;
				break;
			case 'v':
				return version ();
			case 'H':
				hybrid_shutdown = TRUE;
				break;
			case 'E':
				return usage_shutdown ();
			default:
				fprintf (stderr, "Try `%s --help' for more information.\n", myname);
				return 1;
		}
	}

	// Some basic checking for invalid use
	if (install_updates && !at_least_vista)
	{
		fprintf(stderr, "Install Windows Updates not supported.\n");
		fprintf(stderr, "At least Windows Vista or Server 2008 required!\n");
		return 1;
	}

	if (install_updates && force_exitex)
	{
		fprintf(stderr, "-i and -x can not be used together.\n");
		return 1;
	}

	if (install_updates && action != EWX_POWEROFF && action != EWX_REBOOT)
	{
		fprintf(stderr, "-i can only be used with -r or -h.\n");
		return 1;
	}

	if (hybrid_shutdown && action != EWX_POWEROFF)
	{
		fprintf(stderr, "Hybrid mode can only be used with shutdown.\n");
		return 1;
	}

	if (install_updates && hybrid_shutdown)
	{
		fprintf(stderr, "-i and -H can not be used together.\n");
		return 1;
	}

	if (action != ABORT)
	{
		if (optind >= argc)
		{
			fprintf (stderr, "%s: missing arguments\n", myname);
			fprintf (stderr, "Try `%s --help' for more information.\n", myname);
			return 1;
		}
		arg = argv[optind];
		if (!strcasecmp (arg, "now"))
		{
			secs = 0;
			strcpy (timebuf, "NOW");
		}
		else if (arg[0] == '+' && isdigit ((unsigned) arg[1]))
		{
			/* Leading `+' means time in minutes. */
			secs = strtol (arg, &endptr, 10) * 60;
			if (*endptr)
				secs = -1;
			else
				sprintf (timebuf, "in %ld minute", secs / 60);
		}
		else if (isdigit ((unsigned) arg[0]) && strchr (arg + 1, ':'))
		{
			/* HH:MM, timestamp when to shutdown. */
			long hour, minute;
			time_t now, then;
			struct tm *loc;

			hour = strtol (arg, &endptr, 10);
			if (*endptr == ':' && hour >= 0 && hour <= 23)
			{
				minute = strtol (endptr + 1, &endptr, 10);
				if (!*endptr && minute >= 0 && minute <= 59)
				{
					then = now = time (NULL);
					loc = localtime (&now);
					if (loc->tm_hour > hour || (loc->tm_hour == hour && loc->tm_min >= minute))
					{
						then += 24 * 60 * 60; /* Next day */
						loc = localtime (&then);
					}
					loc->tm_hour = hour;
					loc->tm_min = minute;
					loc->tm_sec = 0;
					then = mktime (loc);
					secs = then - now;
					sprintf (timebuf, "at %02ld:%02ld", hour, minute);
				}
			}
		}
		else if (isdigit ((unsigned) arg[0]))
		{
			/* otherwise time in seconds. */
			secs = strtol (arg, &endptr, 10);
			if (*endptr)
				secs = -1;
			else
				sprintf (timebuf, "in %ld seconds", secs);
		}
		if (secs < 0)
		{
			fprintf (stderr, "%s: Invalid time format.\n", myname);
			fprintf (stderr, "Try `%s --help' for more information.\n", myname);
			return 2;
		}
    }

	// Are there any arguments left? That must the user supplied message
	for (int i = optind + 1; i < argc; i++)
	{
		if (strlen(msgbuf) > 0)
			strncat(msgbuf, " ", MAXBUF);
		strncat(msgbuf, argv[i], MAXBUF);
	}

	return -1;
}

// Parse the command line for the short commands
int parse_cmdline_reboot(int argc, char **argv)
{
	struct option longopts[] = {
		{"exitex", no_argument, NULL, 'x'},
		{"force", no_argument, NULL, 'f'},
		{"install", no_argument, NULL, 'i'},
		{"hybrid", no_argument, NULL, 'H'},
		{"help", no_argument, NULL, 'E'},
		{"version", no_argument, NULL, 'v'},
		{0, no_argument, NULL, 0}
	};

	char opts[] = "xfi";
	int c;

	while ((c = getopt_long (argc, argv, opts, longopts, NULL)) != EOF)
	{
		switch (c)
		{
			case 'f':
				force = EWX_FORCE;
				break;
			case 'i':
				install_updates = TRUE;
				break;
			case 'x':
				force_exitex = TRUE;
				break;
			case 'H':
				hybrid_shutdown = TRUE;
				break;
			case 'v':
				return version ();
			case 'E':
				return usage_reboot ();
			default:
				fprintf (stderr, "Try `%s --help' for more information.\n", myname);
				return 1;
		}
	}

	if (optind != argc)
	{
		fprintf (stderr, "%s: too many arguments\n", myname);
		fprintf (stderr, "Try `%s --help' for more information.\n", myname);
		return 1;
	}

	// Some basic checking for invalid use
	if (install_updates && action != EWX_POWEROFF && action != EWX_REBOOT)
	{
		fprintf(stderr, "-i can not be used with %s.\n", myname);
		return 1;
	}

	if (hybrid_shutdown && action != EWX_POWEROFF)
	{
		fprintf(stderr, "Hybrid mode can not be used with %s.\n", myname);
		return 1;
	}

	if (install_updates && hybrid_shutdown)
	{
		fprintf(stderr, "-i and -H can not be used together.\n");
		return 1;
	}

	strcpy (timebuf, "NOW");
	return -1;
}

void check_windows_version(void)
{
	// Determine the Windows Version
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);

	at_least_vista = (osvi.dwMajorVersion >= 6);
}

// construct the shutdown message
void construct_msg(void)
{
	// Did the user already give a message on the command line?
	if (strlen(msgbuf) != 0)
		return;

	// Start with this
	strncpy(msgbuf, "WARNING!!! System will ", MAXBUF);

	// Will updates de installed during shutdown/reboot?
	if (install_updates)
		strncat(msgbuf, "install updates and ", MAXBUF);

	// Add the action
	switch (action)
	{
		case EWX_POWEROFF:
			if (hybrid_shutdown)
				strncat(msgbuf, "hybrid ", MAXBUF);
			strncat(msgbuf, "shutdown", MAXBUF);
			break;
		case EWX_REBOOT:
			strncat(msgbuf, "reboot", MAXBUF);
			break;
		case HIBERNATE:
			strncat(msgbuf, "hibernate", MAXBUF);
			break;
		case SUSPEND:
			strncat(msgbuf, "suspend", MAXBUF);
			break;
	}

	// Tell the user when this is going to happen
	strncat(msgbuf, " ", MAXBUF);
	strncat(msgbuf, timebuf, MAXBUF);
}

// Do the shutdown. Depending on the Windows version use the best API call
BOOL do_shutdown(void)
{
	if (!at_least_vista)
	{
		return InitiateSystemShutdownEx(NULL, msgbuf, secs,
										force == EWX_FORCE,
										action == EWX_REBOOT,
										SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
	}

	// InitiateSystemShutdown() is available from Vista/Server 2008 and later
	DWORD dwFlags = 0;
	DWORD dwReason = SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_FLAG_PLANNED;

	// Shutdown
	if (action == EWX_POWEROFF)
		dwFlags |= SHUTDOWN_POWEROFF;

	// Reboot
	if (action == EWX_REBOOT)
		dwFlags |= SHUTDOWN_RESTART;

	// Force
	if (force == EWX_FORCE)
		dwFlags |= SHUTDOWN_FORCE_OTHERS | SHUTDOWN_FORCE_SELF;

	// When installing updates use a different reason
	if (install_updates)
	{
		dwFlags |= SHUTDOWN_INSTALL_UPDATES;
		dwReason |= SHTDN_REASON_MINOR_UPGRADE;
	}
	else
		dwReason |= SHTDN_REASON_MINOR_OTHER;

	// Shutdown in hybrid mode
	if (hybrid_shutdown)
		dwFlags |= SHUTDOWN_HYBRID;

	// Dynamically load InitiateShutdown()
	hLibrary = LoadLibrary("Advapi32.dll");
	if (hLibrary == NULL)
		return FALSE;

	LPINITIATESHUTDOWN lpInitiateShutdown = (LPINITIATESHUTDOWN) GetProcAddress(hLibrary, "InitiateShutdownA");
	if (lpInitiateShutdown == NULL)
	{
		FreeLibrary((HMODULE) hLibrary);
		return FALSE;
	}

	// Perform the shutdown itself
	DWORD ret = lpInitiateShutdown(NULL, msgbuf, secs, dwFlags, dwReason);

	// Close the handle to the DLL
	FreeLibrary(hLibrary);
	return ret == ERROR_SUCCESS;
}

int main (int argc, char **argv)
{
	timebuf[0] = 0;
	msgbuf[0] = 0;

	check_windows_version();

	if ((myname = strrchr (argv[0], '/')) || (myname = strrchr (argv[0], '\\')))
		++myname;
	else
		myname = argv[0];
	if (strrchr (myname, '.'))
		*strrchr (myname, '.') = '\0';
	if (!strcasecmp (myname, "reboot"))
	{
		action = EWX_REBOOT;
		secs = 0;
	}
	else if (!strcasecmp (myname, "halt") || !strcasecmp (myname, "poweroff"))
	{
		action = EWX_POWEROFF;
		secs = 0;
	}
	else if (!strcasecmp (myname, "hibernate"))
	{
		action = HIBERNATE;
		secs = 0;
	}
	else if (!strcasecmp (myname, "suspend"))
	{
		action = SUSPEND;
		secs = 0;
	}

	/* Parse the entire command line */
	if (secs < 0)
	{
		/* secs not set to 0, so command must invoked as "shutdown" */
		int ret = parse_cmdline_shutdown(argc, argv);
		if (ret >= 0)
			return ret;
	}
	else
	{
		int ret = parse_cmdline_reboot(argc, argv);
		if (ret >= 0)
			return ret;
	}

	if (setprivs ())
		return 3;

	construct_msg();
	if (action != ABORT)
		printf ("%s\n", msgbuf);

	/* Execute the action */
	if (action == EWX_POWEROFF || action == EWX_REBOOT)
	{
		if (force_exitex)
		{
			while (secs)
				secs = sleep (secs);
			if (ExitWindowsEx (action | force, 0x80000000))
				return 0;
		}
		else if (do_shutdown())
			return 0;
	}
	else if (action == ABORT)
	{
		if (AbortSystemShutdown(NULL))
			return 0;
	}
	else
	{
		while (secs)
			secs = sleep (secs);
		if (SetSystemPowerState (action == SUSPEND, force == EWX_FORCE))
			return 0;
	}

	/* Something went wrong */
	fprintf (stderr, "%s: Couldn't ", myname);
	switch (action)
	{
		case EWX_POWEROFF:
			fprintf (stderr, "shutdown");
			break;
		case EWX_REBOOT:
			fprintf (stderr, "reboot");
			break;
		case HIBERNATE:
			fprintf (stderr, "hibernate");
			break;
		case SUSPEND:
			fprintf (stderr, "suspend");
			break;
		case ABORT:
			fprintf (stderr, "abort");
			break;
	}
	DWORD err = GetLastError ();
	fprintf (stderr, ": %s\n", error (err));
	return 3;
}

// vim: ts=4:sw=4:noet
