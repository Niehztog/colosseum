// sl_write.c -- <INVENTED FILENAME>. The Standard Log's low-level writers:
// one function per record field, plus the log file's own open/close.

#include "g_local.h"


// gamex86.dll: 10063130..10063164
// gamei386.so: 00073B68..00073BA5
void sl_LogMapName (game_import_t *import, char *mapname)
{
	char	output[1024];

	sprintf (output, "\t\tMap\t%s", mapname);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 10063164..1006319D
// gamei386.so: 00073BA8..00073C0D
void sl_LogGameStart (game_import_t *import, float time)
{
	char	output[1024];

	sprintf (output, "\t\tGameStart\t\t\t%d", (int)time);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 1006319D..100631CD
// gamei386.so: 00073C10..00073C4A
void sl_LogVers (game_import_t *import)
{
	char	output[1024];

	sprintf (output, "\t\tStdLog\t1.2");
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 100631CD..10063201
// gamei386.so: 00073C4C..00073C89
void sl_LogPatch (game_import_t *import, char *patch)
{
	char	output[1024];

	sprintf (output, "\t\tPatchName\t%s", patch);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 10063201..1006331F
// gamei386.so: 00073C8C..00073DA2
void sl_LogDate (game_import_t *import)
{
	char		output[1024];
	time_t		tval;
	struct tm	*lt;
	char		*month;
	int			monthnum;

	time (&tval);
	lt = localtime (&tval);
	monthnum = lt->tm_mon;

	if (monthnum == 0)
		month = "Jan";
	else if (monthnum == 1)
		month = "Feb";
	else if (monthnum == 2)
		month = "Mar";
	else if (monthnum == 3)
		month = "Apr";
	else if (monthnum == 4)
		month = "May";
	else if (monthnum == 5)
		month = "Jun";
	else if (monthnum == 6)
		month = "Jul";
	else if (monthnum == 7)
		month = "Aug";
	else if (monthnum == 8)
		month = "Sep";
	else if (monthnum == 9)
		month = "Oct";
	else if (monthnum == 10)
		month = "Nov";
	else
		month = "Dec";

	sprintf (output, "\t\tLogDate\t%.2d %s %d", lt->tm_mday, month,
		lt->tm_year + 1900);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 1006331F..1006337E
// gamei386.so: 00073DA4..00073DF8
void sl_LogTime (game_import_t *import)
{
	char		output[1024];
	time_t		t;
	struct tm	*tmp;

	time (&t);
	tmp = localtime (&t);
	sprintf (output, "\t\tLogTime\t%.2d:%.2d:%.2d", tmp->tm_hour, tmp->tm_min,
		tmp->tm_sec);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 1006337E..100633B2
// gamei386.so: 00073DF8..00073E35
void sl_LogDeathFlags (game_import_t *import, unsigned long flags)
{
	char	output[1024];

	sprintf (output, "\t\tLogDeathFlags\t%lu", flags);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 100633B2..100633EB
// gamei386.so: 00073E38..00073E9D
void sl_LogGameEnd (game_import_t *import, float time)
{
	char	output[1024];

	sprintf (output, "\t\tGameEnd\t\t\t%d", (int)time);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 100633EB..10063406
// gamei386.so: 00073EA0..00073ECA
void sl_CloseLogFile (void)
{
	ngLog_logClose (0, 0);
	sl_ngloglog_status = 0;
}

// gamex86.dll: 10063406..10063443
// gamei386.so: 00073ECC..00073F35
void sl_LogPlayerConnect (game_import_t *import, char *name, int unused,
	float time)
{
	char	output[1024];

	sprintf (output, "\t\tPlayerConnect\t%s\t\t%d", name, (int)time);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 10063443..10063480
// gamei386.so: 00073F38..00073FA1
void sl_LogPlayerLeft (game_import_t *import, char *name, float time)
{
	char	output[1024];

	sprintf (output, "\t\tPlayerLeft\t%s\t\t%d", name, (int)time);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 10063480..100634C1
// gamei386.so: 00073FA4..00074010
void sl_LogPlayerRename (game_import_t *import, char *oldname, char *newname,
	float time)
{
	char	output[1024];

	sprintf (output, "\t\tPlayerRename\t%s\t%s\t%d", oldname, newname,
		(int)time);
	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 100634C1..1006354B
// gamei386.so: 00074010..000740E3
void sl_LogScore (game_import_t *import, char *event, char *player, char *name,
	char *team, int score, float time, int ping)
{
	char	output[1024];

	if (!player)
		sprintf (output, "%s\t\t%s\t%s\t%d\t%d\t%d", event, name, team, score,
			(int)time, ping);
	else
		sprintf (output, "%s\t%s\t%s\t%s\t%d\t%d\t%d", event, player, name,
			team, score, (int)time, ping);

	q2log_stdlog_logWrite (output);
}

// gamex86.dll: 1006354B..100636C2
// gamei386.so: 000740E4..00074303
int sl_OpenLogFile (game_import_t *import)
{
	if (sl_ngloglog_status)
	{
		sl_ngloglog_status = 2;
		return 2;
	}

	sl_log_method = gi.cvar ("sl_log_method", "0", 0);
	sl_filename = gi.cvar ("sl_filename", "stdlog.log", 0);
	sl_log_style = gi.cvar ("sl_log_style", "0", 0);
	sl_log_flush = gi.cvar ("sl_log_flush", "2", 0);

	// One `||` guard of the form `A || (B && C)`, not two separate early
	// returns.
	if (!(int)sl_log_method->value ||
		(ngloglog_status && !(int)nglog_worldstats->value))
	{
		sl_ngloglog_status = 0;
		return 0;
	}

	strcpy (__nglog_logname, sl_filename->string);
	__nglog_logstyle = 1;
	__nglog_buffer = 40;

	if (!(int)sl_log_flush->value)
		__nglog_flush = 2;
	else if ((int)sl_log_flush->value == 1)
		__nglog_flush = 1;
	else
		__nglog_flush = 0;

	if (ngLog_init ())
	{
		q2log_stdlog_showErrors ();
		sl_ngloglog_status = 0;
		return 0;
	}

	q2log_stdlog_showErrors ();
	sl_ngloglog_status = 1;
	gi.dprintf ("Standard Log logging enabled.\n");

	return 1;
}

// gamex86.dll: 100636C2..100636E5
// gamei386.so: 00074304..00074333
void q2log_stdlog_logWrite (char *line)
{
	if (!sl_ngloglog_status)
		return;

	ngLog_logWrite (line, 1);
	q2log_stdlog_showErrors ();
}

// gamex86.dll: 100636E5..10063730
// gamei386.so: 00074334..0007438B
void q2log_stdlog_showErrors (void)
{
	int		i;

	for (i = 0; i < __nglog_num_errs; i++)
		gi.dprintf ("%s", __nglog_error_msg[i]);
}
