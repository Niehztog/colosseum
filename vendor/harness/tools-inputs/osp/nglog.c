// nglog.c -- <INVENTED FILENAME>. The ngLog / ngWorldStats file writer.
//
// It owns two output files: log 1 is the plain ngLog text log (`log_file`,
// gated by __nglog_logstyle) and log 2 is the obfuscated ngWorldStats log
// (`worldlog_file`, gated by __nglog_worldlog).  Callers pass 1, 2 or
// 0-for-both as the `which` argument.
//
// The ngWorldStats format is deliberately unreadable on disk: every line is
// fed through MD5 to accumulate a running checksum and then XOR-scrambled a
// byte at a time (ngLog_inputLine), and the running checksum is emitted at
// close as a hex "mark" (ngLog_giveMark) so ngWorldStats can tell a doctored
// log from a real one.  ngLog_transMark de-obfuscates the 0x21-byte salt table
// in .rodata the same way, with a different key.
//
// _WIN32_WINNT is defined below so that windows.h declares gethostname and
// gethostbyname through winsock2.h, i.e. with __declspec(dllimport); without
// it the two calls go through linker-synthesised thunks instead of the IAT.
// No other TU in this tree calls a Winsock function.
#define _WIN32_WINNT 0x0400
#include "g_local.h"
#include <errno.h>

int	buffer_lines = 0;
int	wbuffer_lines = 0;
FILE * log_file = NULL;
FILE * worldlog_file = NULL;
int	__nglog_worldlog = 0;
int	__nglog_num_errs = 0;
int	__nglog_ngstats_exec = 0;
int	ngloglog_status = 0;
char	__nglog_error_msg[8][4096];
char	__nglog_ngstats_cfg[1024];
char	__nglog_worldlog_tag[64];
char	__nglog_worldlog_prefix[1024];
char	__nglog_rel_path[1024];
char	__nglog_logpath[1024];
int	__nglog_buffer;
int	__nglog_flush;
char	__nglog_log_prefix[1024];
char	__nglog_logname[1024];
int	__nglog_logstyle;
char	__nglog_ngstats_logdir[1024];
char	__nglog_worldlog_path[1024];
char	__nglog_worldlog_name[1024];
MD5_CTX	context;
cvar_t * nglog_worldstats;
cvar_t * nglog_flush;
cvar_t * nglog_ngstats_browser;
cvar_t * nglog_logchat;
cvar_t * nglog_logstyle;
cvar_t * ngWorldStats_Status;
cvar_t * nglog_ngstats_exec;
cvar_t * nglog_logstyle_working;
cvar_t * nglog_ngstats_cfg;
cvar_t * nglog_logallpickups;
cvar_t * nglog_ngstats_vidrestart;
cvar_t * nglog_buffer;
cvar_t * nglog_logname;
cvar_t * nglog_ngstats_logdir;
cvar_t * nglog_logmiscpickup;


// gamex86.dll: 1004CE60..1004D2AD
// gamei386.so: 0006E3F8..0006E7B9
int ngLog_init (void)
{
	char	opmode[8];
	char	stamp[64];
	int		exists;

	if (log_file || worldlog_file)
		ngLog_logClose (0, NULL);

	ngLog_errorMsgClear ();
	ngLog_initMark ();

	if (__nglog_logstyle == 1 || __nglog_logstyle == atoi ("5"))
		strcpy (opmode, "a+");
	else if (__nglog_logstyle == 2)
		strcpy (opmode, "w");
	else if (__nglog_logstyle == 3)
	{
		strcpy (opmode, "w");
		ngLog_rotateFile ();
	}
	else if (__nglog_logstyle == 4)
	{
		strcpy (opmode, "w");
		strcpy (__nglog_logname, __nglog_logpath);
		ngLog_getDateInfo (stamp, 1);
		strcat (__nglog_logname, stamp);
		strcat (__nglog_logname, ".");
		strcat (__nglog_logname, __nglog_worldlog_tag);
		strcpy (__nglog_log_prefix, __nglog_logname);
		strcat (__nglog_logname, ".tmp");
	}
	else
	{
		sprintf (__nglog_error_msg[__nglog_num_errs++],
			"ngLog logging disabled.\n");
		if (!__nglog_worldlog)
			return -1;
	}

	if (__nglog_logstyle)
	{
		exists = ngLog_fileExists (__nglog_logname);

		if (exists == 2)
		{
			sprintf (__nglog_error_msg[__nglog_num_errs++],
				"Error on opening %s, %d (%d)\n", __nglog_logname, errno,
				__nglog_logstyle);
			if (!__nglog_worldlog)
				return -1;
		}
	}

	if (__nglog_logstyle)
	{
		log_file = fopen (__nglog_logname, opmode);
		if (!log_file)
		{
			sprintf (__nglog_error_msg[__nglog_num_errs++],
				"Couldn't create logfile %s: %d (%d)\n", __nglog_logname, errno,
				__nglog_logstyle);
			if (!__nglog_worldlog)
				return -1;
		}
	}

	if (__nglog_logstyle == 4)
		sprintf (__nglog_error_msg[__nglog_num_errs++],
			"ngStats logging enabled.\n");

	if (__nglog_worldlog)
	{
		strcpy (__nglog_worldlog_name, __nglog_worldlog_path);
		ngLog_getDateInfo (stamp, 1);
		strcat (__nglog_worldlog_name, stamp);
		strcat (__nglog_worldlog_name, ".");
		strcat (__nglog_worldlog_name, __nglog_worldlog_tag);
		strcpy (__nglog_worldlog_prefix, __nglog_worldlog_name);
		strcat (__nglog_worldlog_name, ".tmp");
		worldlog_file = fopen (__nglog_worldlog_name, "w");
		if (!worldlog_file)
		{
			sprintf (__nglog_error_msg[__nglog_num_errs++],
				"*** Couldn't create ngWorldStats logfile %s: %d\n",
				__nglog_worldlog_name, errno);
			sprintf (__nglog_error_msg[__nglog_num_errs++],
				"ngWorldStats logging disabled.\n");
			if (!log_file || !__nglog_logstyle)
				return -1;
		}
		else
			sprintf (__nglog_error_msg[__nglog_num_errs++],
				"ngWorldStats logging enabled.\n");
	}
	else
		sprintf (__nglog_error_msg[__nglog_num_errs++],
			"ngWorldStats logging disabled.\n");

	return 0;
}

/*
==============
ngLog_logWrite

Append one line to whichever of the two logs `which` selects (0 = both).
==============
*/
// gamex86.dll: 1004D2AD..1004D52A
// gamei386.so: 0006E7BC..0006E9A9
void ngLog_logWrite (char *line, int which)
{
	char	buf[4096];
	int		len;
	int		wrote;

	if (!log_file && !worldlog_file)
		return;

	sprintf (buf, "%s\n", line);
	len = strlen (buf);
	ngLog_errorMsgClear ();

	if (__nglog_logstyle && log_file && which != 2)
	{
		wrote = fprintf (log_file, "%s", buf);
		if (wrote != len)
		{
			sprintf (__nglog_error_msg[__nglog_num_errs++],
				"Error writing to %s: %d != %d (%d)\n",
				__nglog_logname, wrote, len, errno);
			ngLog_logClose (1, NULL);
			return;
		}

		if (!__nglog_flush)
			ngLog_logFlush (log_file);
		else if (__nglog_flush == 1)
		{
			buffer_lines++;
			if (buffer_lines > __nglog_buffer)
			{
				buffer_lines = 0;
				ngLog_logFlush (log_file);
			}
		}
	}

	if (__nglog_worldlog && worldlog_file && which != 1)
	{
		ngLog_inputLine (buf);
		wrote = fwrite (buf, 1, len, worldlog_file);
		if (wrote != len)
		{
			sprintf (__nglog_error_msg[__nglog_num_errs++],
				"Error writing to ngWorldStats log: %d != %d (%d)\n",
				wrote, len, errno);
			ngLog_logClose (2, NULL);
			return;
		}

		if (!__nglog_flush)
			ngLog_logFlush (worldlog_file);
		else if (__nglog_flush == 1)
		{
			wbuffer_lines++;
			if (wbuffer_lines > __nglog_buffer)
			{
				wbuffer_lines = 0;
				ngLog_logFlush (worldlog_file);
			}
		}
	}
}

// gamex86.dll: 1004D52A..1004D53B
// gamei386.so: 0006E9AC..0006E9C9
void ngLog_logFlush (FILE *f)
{
	fflush (f);
}

/*
==============
ngLog_ngStatsCall

Build the ngStatsQ2T command line. It only formats it -- the caller runs it.
==============
*/
// gamex86.dll: 1004D53B..1004D71D
// gamei386.so: 0006E9CC..0006EAB0
void ngLog_ngStatsCall (int arg)
{
	// The MSVC frame emits the three aggregate initialisers in DECLARATION
	// order, and real's order is si, pi, flag -- which is what puts the Win32
	// pair ahead of `flag` here.  gcc sees neither of them, and expands the
	// initialiser as the same 6-byte block move it gives strcpy of a literal.
	char	cmd[2048];
#ifdef _WIN32
	STARTUPINFO			si = {0};
	PROCESS_INFORMATION	pi = {0};
#endif
	char	flag[6] = "false";
	char	exec[1024];
	char	cfg[1024];
	char	cwd[1024];

	if (arg)
		strcpy (flag, "true");

	strcpy (cwd, __nglog_log_prefix);
	strcat (cwd, ".log");

#ifdef _WIN32
	si.cb = sizeof (si);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	if (arg)
		si.wShowWindow = SW_SHOW;
	else
		si.wShowWindow = SW_HIDE;
	si.hStdInput = NULL;
	si.hStdOutput = NULL;
	si.hStdError = NULL;
#endif

	getcwd (cwd, 1024);

#ifdef _WIN32
	sprintf (exec, "%s\\%s\\ngStats\\ngStatsQ2T.exe", cwd, __nglog_rel_path);
	sprintf (cfg, "%s\\%s\\ngStats\\%s", cwd, __nglog_rel_path, __nglog_ngstats_logdir);
	sprintf (cmd, "%s -b %s -c %s\\%s\\ngStats\\%s %s", exec, flag,
		cwd, __nglog_rel_path, __nglog_ngstats_cfg, cfg);

	// DETACHED_PROCESS for the silent end-of-map run,
	// CREATE_NEW_PROCESS_GROUP for the interactive one.
	if (!arg)
		CreateProcess (NULL, cmd, NULL, NULL, FALSE, DETACHED_PROCESS,
			NULL, NULL, &si, &pi);
	else
		CreateProcess (NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP,
			NULL, NULL, &si, &pi);
#else
	sprintf (exec, "%s/%s/ngStats/bin/ngStatsQ2T", cwd, __nglog_rel_path);
	sprintf (cfg, "%s/%s/ngStats/%s", cwd, __nglog_rel_path, __nglog_ngstats_logdir);
	sprintf (cmd, "%s -b %s -c %s %s &", exec, flag, __nglog_ngstats_cfg, cfg);
#endif
}

/*
==============
ngLog_logClose

Close one or both logs, rename the working file into place when the style asks
for it, and hand the finished log to ngStats / ngWorldStats.
==============
*/
// gamex86.dll: 1004D71D..1004D959
// gamei386.so: 0006EAB0..0006EC2A
void ngLog_logClose (int which, int reason)
{
	char	name[1024];
	char	cmd[2048];
	char	exec[1024];
	char	dir[1024];

	if (which != 2)
	{
		if (log_file)
		{
			fflush (log_file);
			fclose (log_file);
			log_file = NULL;
			if (__nglog_logstyle == 4)
			{
				strcpy (name, __nglog_log_prefix);
				strcat (name, ".log");
				rename (__nglog_logname, name);
			}
		}

		if (__nglog_logstyle == 4 && __nglog_ngstats_exec)
			ngLog_ngStatsCall (reason);
	}

	if (worldlog_file && which != 1)
	{
#ifdef _WIN32
		STARTUPINFO			si = {0};
		PROCESS_INFORMATION	pi = {0};
#endif
		fflush (worldlog_file);
		fclose (worldlog_file);
		worldlog_file = NULL;
		strcpy (name, __nglog_worldlog_prefix);
		strcat (name, ".log");
		rename (__nglog_worldlog_name, name);

#ifdef _WIN32
			// si/pi are declared INSIDE this block: real's zeroing sits partway
			// into the function rather than at entry.
		si.cb = sizeof (si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		si.hStdInput = NULL;
		si.hStdOutput = NULL;
		si.hStdError = NULL;
#endif

		getcwd (name, 1024);
#ifdef _WIN32
		sprintf (exec, "%s\\%s\\ngWorldStats\\bin\\ngWorldStats.exe", name, __nglog_rel_path);
		sprintf (dir, "%s\\%s\\ngWorldStats\\logs", name, __nglog_rel_path);
		sprintf (cmd, "%s -d %s -g Quake2Tourney", exec, dir);
		CreateProcess (NULL, cmd, NULL, NULL, FALSE, DETACHED_PROCESS,
			NULL, NULL, &si, &pi);
#else
		sprintf (exec, "%s/%s/ngWorldStats/bin/ngWorldStats", name, __nglog_rel_path);
		sprintf (dir, "%s/%s/ngWorldStats/logs", name, __nglog_rel_path);
		sprintf (cmd, "%s -d %s -g Quake2Tourney &", exec, dir);
		system (cmd);
#endif
	}
}

// gamex86.dll: 1004D959..1004D9A1
// gamei386.so: 0006EC2C..0006EC78
int ngLog_fileExists (char *name)
{
	FILE	*f;

	f = fopen (name, "r");
	if (!f)
	{
		if (errno == ENOENT)
			return 0;
		return 2;
	}
	fclose (f);
	return 1;
}

// gamex86.dll: 1004D9A1..1004D9DA
// gamei386.so: 0006EC78..0006ECB3
void ngLog_errorMsgClear (void)
{
	int		i;

	for (i = 0; i < 8; i++)
		__nglog_error_msg[i][0] = '\0';
	__nglog_num_errs = 0;
}

/*
==============
ngLog_getDateInfo

`full` picks the long stamp, with the timezone offset in hours as a signed
float; otherwise just down to the minute.

The timezone source is #ifdef'd: on Unix it is struct timeb::timezone, a
short; under Win32 it is the CRT global `timezone`, an int.
==============
*/
// gamex86.dll: 1004D9DA..1004DAB2
// gamei386.so: 0006ECB4..0006ED64
void ngLog_getDateInfo (char *out, int full)
{
	struct timeb	tb;
	time_t			t;
	struct tm		*ltime;
#ifndef _WIN32
	short			tz;
#endif

	ftime (&tb);
#ifndef _WIN32
	tz = tb.timezone;
#endif
	time (&t);
	ltime = localtime (&t);

	if (full)
		sprintf (out, "%d.%.2d.%.2d.%.2d.%.2d.%.2d.%.2d.%+2.1f",
			ltime->tm_year + 1900, ltime->tm_mon + 1, ltime->tm_mday, ltime->tm_hour,
			ltime->tm_min, ltime->tm_sec, tb.millitm,
#ifdef _WIN32
			-(float)(timezone / 3600));
#else
			-(float)(tz / 3600));
#endif
	else
		sprintf (out, "%d.%.2d.%.2d.%.2d.%.2d",
			ltime->tm_year, ltime->tm_mon + 1, ltime->tm_mday, ltime->tm_hour, ltime->tm_min);
}

/*
==============
ngLog_rotateFile

Find the first free `<base>NN.<ext>` and make that the log name.
==============
*/
// gamex86.dll: 1004DAB2..1004DC28
// gamei386.so: 0006ED64..0006EEBF
void ngLog_rotateFile (void)
{
	char	stem[1024];
	char	name[1024];
	char	*suffix;
	int		n;

	n = 0;
	strcpy (stem, __nglog_logname);
	suffix = strrchr (stem, '.');
	if (suffix)
	{
		*suffix = '\0';
		suffix++;
	}

	strcpy (name, stem);
	if (suffix)
	{
		strcat (name, ".");
		strcat (name, suffix);
	}

	while (ngLog_fileExists (name) == 1)
	{
		if (suffix)
			sprintf (name, "%s%.2d.%s", stem, n, suffix);
		else
			sprintf (name, "%s%.2d", stem, n);
		n++;
	}

	strcpy (__nglog_logname, name);
	sprintf (__nglog_error_msg[__nglog_num_errs++], "Writing to log %s\n",
		__nglog_logname);
}

/*
==============
ngLog_hostAddr

This machine's dotted-quad, or an error string in the same buffer.
==============
*/
// gamex86.dll: 1004DC28..1004DCE0
// gamei386.so: 0006EEC0..0006EF58
char *ngLog_hostAddr (void)
{
	static char		addr[128];
	static char		host[256];
	struct hostent	*h;
	unsigned char	*hostaddr;	// <INVENTED NAME>

	if (gethostname (host, 256))
	{
		sprintf (addr, "ERROR: no name");
		return addr;
	}

	h = gethostbyname (host);
	if (!h)
	{
		sprintf (addr, "ERROR: can't convert name\n");
		return addr;
	}

	hostaddr = (unsigned char *)h->h_addr_list[0];
	sprintf (addr, "%d.%d.%d.%d",
		hostaddr[0], hostaddr[1], hostaddr[2], hostaddr[3]);
	return addr;
}
