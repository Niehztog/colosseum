// osp_maps.c -- <INVENTED FILENAME>. maps.txt: the map queue, its parser and
// the next-map picker.

#include "g_local.h"
#include <sys/timeb.h>

map_t *	map = NULL;
unsigned	map_size = 0;

// The queue cursor -- a file-static, <INVENTED NAME>.
static int		cur_map = 0;

int	selected_map = 0;
int	next_map;


// Picks the next level and returns a freshly spawned target_changelevel for it,
// or NULL when map_queue is off / there is nothing to pick. EndDMLevel is the
// caller.
// gamex86.dll: 10062530..10062C05
// gamei386.so: 0006D688..0006DE2E
edict_t *NextMap (void)
{
	struct timeb	tb;
	edict_t			*ent = NULL;
	int				players = 0;
	int				found = 0;
	int				pass = 0;
	cvar_t			*map_queue = gi.cvar ("map_queue", "1", 0);
	cvar_t			*map_random = gi.cvar ("map_random", "1", 0);
	cvar_t			*map_once = gi.cvar ("map_once", "1", 0);
	cvar_t			*map_debug = gi.cvar ("map_debug", "0", 0);
	cvar_t			*map_nocount = gi.cvar ("map_nocount", "0", 0);
	int				i;
	int				n;
	edict_t			*cl_ent;

	ftime (&tb);

	if (!(int)map_queue->value)
		return NULL;

	if (!map)
		OSP_loadMaps ();

	if (map_size && !selected_map)
	{
		// where are we now?
		for (i = 0; i < map_size; i++)
		{
			if (!strcmp (level.mapname, map[i].name))
			{
				cur_map = i;
				map[i].used = 1;
				break;
			}
		}

		if (!map_once || !(int)map_once->value)
			for (i = 0; i < map_size; i++)
				map[i].used = 0;

		do
		{
			if (map_random && (int)map_random->value)
			{
				// srand (tb.time), not tb.millitm -- the original reads the
				// whole seconds field here, not the milliseconds.
				srand (tb.time);
				cur_map = rand () % map_size;
				if (map_debug && (int)map_debug->value)
					gi.dprintf ("Random Map %d %s\n", cur_map, map[cur_map].name);
			}

			if (!(map_once && (int)map_once->value) &&
				!(map_random && (int)map_random->value))
				n = (cur_map + 1) % map_size;
			else
				n = cur_map;

			players = 0;
			for (i = 1; i <= game.maxclients; i++)
			{
				cl_ent = g_edicts + i;
				if (cl_ent->inuse && cl_ent->client &&
					cl_ent->client->pers.connected &&
					!(cl_ent->flags & FL_OSP_BOT))
					players++;
			}

			// walk forward from n, wrapping, until we are back where we started
			do
			{
				if (!map[n].used)
				{
					if ((map[n].minplayers <= players && map[n].maxplayers >= players) ||
						map[n].minplayers == 0 ||
						(players == 0 && map[n].minplayers == 1) ||
						(int)map_nocount->value)
					{
						cur_map = n;
						n = -1;
						found = 1;
						if (map_debug && (int)map_debug->value)
							gi.dprintf ("Map Found %s [fVisited = %d]\n",
										map[cur_map].name, map[cur_map].used);
					}
					else
						n = (n + 1) % map_size;
				}
				// The step is written out TWICE, once per else arm.
				else
					n = (n + 1) % map_size;
			} while (n != -1 && n != cur_map);

			if (n == cur_map)
			{
				// nothing in bounds -- forget the visited flags and take the
				// first thing that fits on a second sweep
				if (map_debug && (int)map_debug->value)
					gi.dprintf ("Map could not be found\n");

				if (map_once && (int)map_once->value > 0)
				{
					if (map_debug && (int)map_debug->value)
						gi.dprintf ("Clearing Visited flags\n");
					for (i = 0; i < map_size; i++)
						map[i].used = 0;
				}

				// if/else, not a ternary, and the sign fix is written out rather
				// than calling abs().
				if (map_random && (int)map_random->value)
					n = rand () % map_size;
				else
					n = 0;
				if (n < 0)
					n = 0 - n;

				for (i = 0; i < map_size; i++, n++)
				{
					if (n >= map_size)
						n = 0;
					if ((map[n].minplayers <= players && map[n].maxplayers >= players) ||
						map[n].minplayers == 0 ||
						(players == 0 && map[n].minplayers == 1) ||
						(int)map_nocount->value)
					{
						cur_map = n;
						break;
					}
				}
				found = 1;
			}
			pass++;
		} while (!found && pass < 2);
	}
	else if (map_size && selected_map)
	{
		found = 1;
		selected_map = 0;
		cur_map = next_map;
		gi.bprintf (PRINT_HIGH, "Next map: %s\n", map[cur_map].name);
	}
	else
		cur_map = 0;

	if (found && !ent)
	{
		if (map_once && (int)map_once->value > 0)
			map[cur_map].used = 1;

		ent = G_Spawn ();
		if (ent)
		{
			ent->classname = "target_changelevel";
			ent->map = map[cur_map].name;
			if (map_debug && (int)map_debug->value)
			{
				gi.dprintf ("MAP CHANGE: %d ", cur_map);
				gi.dprintf (map[cur_map].name);
				gi.dprintf (" [min = %d,max = %d, players = %d]\n",
							map[cur_map].minplayers, map[cur_map].maxplayers,
							players);
			}
		}
	}

	return ent;
}

// gamex86.dll: 10062C05..10062DFA
// gamei386.so: 0006DE30..0006E056
void OSP_loadMaps (void)
{
	FILE	*f = NULL;
	cvar_t	*gamedir;
	cvar_t	*basedir;
	cvar_t	*mfile;

	gamedir = gi.cvar ("gamedir", "tourney", 0);
	basedir = gi.cvar ("basedir", ".", 0);
	{
		// No cached pointer for the default name: the literal is repeated.
		mfile = gi.cvar ("map_file", "maps.txt", 0);
		map_size = 0;

		if (gamedir && basedir)
		{
			{
				char	path[64] = {0};
				char	*pathptr = path;

				sprintf (path, "%s/%s/", basedir->string, gamedir->string);
				if (mfile)
					strcat (path, mfile->string);
				else
					strcat (path, "maps.txt");

				f = fopen (pathptr, "r");
				if (f)
				{
					map_t	record;
					int		ret;
					map_t	*newmap;

					gi.dprintf ("Loading maps from \"%s\"\n", mfile->string);

					do
					{
						record.minplayers = 0;
						record.maxplayers = 0;
						record.used = 0;
						ret = read_map_entry (f, record.name, &record.minplayers,
											  &record.maxplayers);
						if (ret >= 1)
						{
							newmap = (map_t *)realloc (map,
													(map_size + 1) * sizeof(map_t));
							if (newmap)
							{
								map = newmap;
								memcpy (&map[map_size], &record, sizeof(record));
								map_size++;
							}
						}
					} while (ret >= 0);

					fclose (f);
				}
				else
					gi.dprintf ("ERROR: Could not open maps list file [%s]\n", pathptr);
			}
		}
	}
}

// One line of maps.txt, tokenised a character at a time: `<map name> [min]
// [max]`, `#` starts a comment, `"` toggles quoting and `\r` is dropped.
// Returns the number of fields parsed, or -1 at end of file with nothing read.
// gamex86.dll: 10062DFA..10062FA7
// gamei386.so: 0006E058..0006E23F
int read_map_entry (FILE *f, char *name, int *lo, int *hi)
{
	int		field = 0;
	int		len = 0;
	int		quote = 0;
	char	tok[64] = {0};
	char	*p = tok;
	int		c;

	do
	{
		c = fgetc (f);

		if (len > 0 && (((c == ' ' || c == '\t') && !quote) ||
						c == -1 || c == '\n'))
		{
			tok[len] = 0;
			switch (field)
			{
			case 0:
				strncpy (name, p, 64);
				break;
			case 1:
				*lo = atoi (p);
				break;
			case 2:
				*hi = atoi (p);
				break;
			}
			len = 0;
			field++;
		}
		else
		{
			switch (c)
			{
			case '"':
				quote = 1 - quote;
				break;
			case '#':
				if (!quote)
					while (c != -1 && c != '\n')
						c = fgetc (f);
				break;
			case '\r':
				break;
			case ' ':
			case '\t':
				if (!quote)
					break;
			default:
				if (len < 63)
					tok[len++] = c;
			}
		}
	} while (c != -1 && c != '\n');

	if (c == -1 && !field)
		return -1;
	return field;
}

// `set` also latches next_map/selected_map, which is what the vote and the
// admin menu use to force a specific level.
// gamex86.dll: 10062FA7..10063067
// gamei386.so: 0006E240..0006E318
qboolean OSP_mapExists (edict_t *ent, char *name, qboolean set)
{
	unsigned	i;

	if (!map)
	{
		OSP_loadMaps ();
		if (!map)
		{
			if (ent)
				gi.cprintf (ent, PRINT_HIGH, "Sorry, no maps available!\n");
			return false;
		}
	}

	for (i = 0; i < map_size; i++)
	{
		if (!strcmp (name, map[i].name))
		{
			if (set)
			{
				next_map = i;
				selected_map = 1;
			}
			return true;
		}
	}

	if (ent)
		gi.cprintf (ent, PRINT_HIGH, "Sorry, \"%s\" is not available!\n", name);
	return false;
}

// gamex86.dll: 10063067..10063130
// gamei386.so: 0006E318..0006E3F5
void OSP_mapList (edict_t *ent)
{
	unsigned	i;

	if (!map)
		OSP_loadMaps ();

	if (!map || !map_size)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, no maps available!\n");
		return;
	}

	gi.cprintf (ent, PRINT_HIGH, "\nAvailable maps:\n");
	gi.cprintf (ent, PRINT_HIGH, "---------------\n");
	for (i = 0; i < map_size; i++)
		gi.cprintf (ent, PRINT_HIGH, "%s\n", map[i].name);
	gi.cprintf (ent, PRINT_HIGH, "\n");
}
