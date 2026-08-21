OSP Tourney DM v(2.5)
----------------------
http://www.orangesmoothie.org/
rhea@orangesmoothie.org

(Alternate site: http://www.planetquake.com/osp/tourney)
13:53-6.0GMT (01:53 PM, CST), 01 Jul 1999

[ Lastest revisions of this document and OSP Tourney DM server-side .dlls
  and distributions can be found at: http://www.orangesmoothie.org/

  We also have a channel open for IRC discussions on ETG (EnterTheGame)
  under #osp.  Come join in on discussions on OSP Tourney DM!  Info on
  ETG can be found at: http://www.enterthegame.com ]

********************* Installation Instructions *************************

Unzip/untar into your Quake II directory making sure to maintain directory
structure and overwrite existing files.  Follow the instructions in the
Server setup section below.

*************************************************************************


*******   NOTE: OSP Tourney DM has built in support for the new
*******   _ngWorldStats_ system from NetGames USA.  Visit:
*******   http://Quake2.ngWorldStats.com/FAQ for details.
*******
*******   OSP Tourney also has built-in support for the new _ngStats_
*******   local ngLog-format logging system.
*******   Visit: http://www.netgamesusa.com/ngStats for more details.
*******   You can also refer to the documentation found in the
*******   NetGamesUSA.com/ngStats directory for further help (under your
*******   main Quake2/ directory).



*** Welcome to OSP Tourney DM!! ***



Overview:
=========
OSP Tourney DM (referred to as "Tourney" herein) is a server-side only
mod designed to facilitate match play for tournaments - whether
LAN-based, online, or single-player with server-controlled "bots".  It
also has an enhanced regular DM mode for relaxed warmups and quick pickup
Free-For-ALL (FFA) play.

Rather than having specific mods used for various aspects of tournament
play (i.e. Battle/GX for team play, Duel for 1v1, and even id DM for FFA
qualifiers), OSP Tourney DM was designed specifically to handle these
modes of play with all of their unique requirements.  Whether in a
competitive multi-player environment or for the single player in
matches against server "bots" of various degrees of difficulty, OSP
Tourney DM can be easily used in either setting.

Note: OSP Tourney DM can now easily be used as a Single-Player Quake2
game modification with the addition of the Tourney Launcher/Config Editor,
server-side Gladiator bots, and built-in ngStats HTML-based stats tracking
integration for all modes of play.  No other Quake2 mod has ever had this
level of integration.

OSP Tourney DM supports 4 "modes" of operation:
	- Regular DeathMatch
	- Qualifier DeathMatch
	- Team DM Play
	- 1v1 DM Play

It should be noted that each of these modes are unique and are not just
simple hacks to give different modes of play.  OSP Tourney DM is in fact
4 separate mods in one.

To create a smoother and more configurable way to play competitive
QuakeII gameplay, the below listed characteristics make playing more
enjoyable for players and server admins alike:

* 4 separate modes of operation: Normal DM, Qualifier DM, Team, 1v1
* Highly customized HUD giving critical information about the current
  game to players at all times
* Advanced map rotation facility to load maps according to the current
  number of players logged into a server either in a random or linear
  fashion (configurable).
* Completely revamped networking architecure to improve network
  efficiency and reduce the probability of the dreaded "client overflow".
* Complete client menuing/help system
* Integrated world-wide stat tracking system utilitizing ngWorldStats.
  Visit http://Quake2.ngWorldStats.com/ for complete information.
* Integrated local stat parsing and player tracking system utilizing
  NetGamesUSA's ngStats processing system.  Visit:
  http://www.netgamesusa.com/ngStats/Quake2 for complete information.
* MrElusive's phenomenonal Gladiator server-side bots.  These bots are
  available in all of OSP Tourney's gameplay modes.  The Gladiator bots
  have a highly advanced AI subsystem and can also be given and follow
  orders from real players.  Bots can be voted in, loaded every map
  change, or even leave/join as more real clients join/leave.
* Auxillary server configuration utility developed by Luis Toscano to
  easily handle all of OSP Tourney's various settings.  It can even be
  used as a front-end to launching a Tourney server.
* Ability for clients to vote in different (predefined) server configs.
  It is now possible for a single server to host any variation of server
  types (1v1, team, regular DM, railgun-only, etc.) without having to
  run multiple servers.
* Playername reservation system based on either passwords or IP address
  ranges.
* High score tracking under FFA (regular DM) modes of play.  The high
  scores recorded are the winners of a given map, absed on FPH (frags
  per hour) or total frags, depending on whether if there is a fraglimit
  set.
* Weapon accuracy statistics throughout and at the end of each round.
  Accuracy stats from previous round are also available.
* Optional client recovery mechanism if they get disconnected due to an
  overflow during the course of a match.  If clients reconnect while the
  match is still in progress, they will recover all of their stats
  (frags, deaths,etc.).  If in team mode, they will also be automatically
  placed back on their original team.
* Client-side bot protection capabilites (including, but not limited to,
  the ZBOT)
* Client bandwidth management controls (rate/fps capping, min/max ping).
* Optional/Votable Lithium-style runes available that are completely
  configurable to normal Lithium style and with some new options as well.
* All client options available from either console or menu (partial)
* Highly configurable voting system from both the console and menu that
  allows players to vote on match parameters such as: time/fraglimits,
  maps, hook, item/weapon banning, certain dmflag settings, team/self
  hurt allowances, server configs, player kicking, runes, and Gladiator
  bots.
* Adjustable behind-the-shoulder/in-eyes chasecam and free-floating
  observer modes.
* Paul Jordan's unique "autocam" for a more realistic following of the
  action during a match.
* Referee class for administrative control during a match
* Special referee class differentiation for players and observes
* Referee-specific and referee-enhanced commands
* Real-time pausing of matches in progess, by referees, with chat
  enabled
* Captain status for teamplay that allows for greater control over team
  members
* Player "time-out" features for teamplay/1v1
* Optional match pausing during duel matches (1v1/teamplay) if a client
  gets disconnected due to an overflow, and waits for them to reconnect
  to continue the match.
* Optional highly configurable grappling hook to spice up gameplay :)
* Automatic demo recording and end-of-level screenshot capabilities
* Scoreboards tailored to mode of play for diverse statistic information
* Scoreboard from previous match also available at any time.
* Anti-telefrag feature to prevent telefrag meltdowns at the start of
  matches with a lot of players.
* Optional player identification tagging facility.  Additional
  enhancements are included that prevent viewing players through water
  surfaces and unique identification of foes and teammates. 
* All-weapon warmup mode for "synced" matches (Qualifier DM, Team, and
  1v1)
* On "synced" modes, all pertinent match info is displayed before match
  begins
* Audible alarms at dicrete points of time remaining in the match
* Many teamplay-specific commands
* Clients can set special client-side variables that allows them to
  autoconfigure their team (name, skin, joincode) upon connecting and
  joining a new match.
* Team-related information is kept across level loads so when the next
  map starts, all you have to do is join and you will be automatically
  set up.  In fact, clients can configure variables even before
  connecting to specify their unique team parameters.
* Integrated tie-breaker mechanism to sort out players who have equal
  scores in FFA mode
* 2 overtime modes for teamplay/1v1: Timed overtime rounds or sudden
  death (first death decides).
* CTF-based "%" context messages that allow players to dynamically
  communcate certain types of status information to their teammates
  (i.e. health, armor, weapon, location, viewable players).
* Settable team join codes for late connecting clients to use during a
  teamplay match to join in on the action.
* End of level music - Reg/Qual DM: Random songs, Team/1v1: winner=cool
  Xian music, loser=laughed at by Makron :)
* Configurable item respawn timing (regular DM only)
* Power armor configuration (regular DM only)
* Client respawn protect options (regular DM only)
* Selectable banned item list for admin
* Selectable start weapon/item and default weapon
* Configurable maximums on player inventory capacities
* Customizable Message of the Day banner on client connect
* ngLog 1.2-compliant logging.  Also includes ngWorldStats logging
  capabilites.
* Standard Logging 1.2 compatible for local or remote logging
* Configurable delayed force respawn option to give a "breather" for
  players before being automatically put back into play.
* 3.20-format compliant for IP banning/allow of clients
* Playername allow/deny ban list capabilites.
* 3.20-format compliant chat muzzling for chat flooders (not enforced
  for intra-team talk)
* Adjustable client-side HUD layouts to accomodate smaller viewsizes.
* Optional fast weapon switching configuration
* Optional client "rate" setting feature
* Optional use of player ID #'s rather than player names to specify a
  connected player.
* Console timestamps that give local server information for additional
  information in qconsole.log logs.
* Configurable gib count to increase the blood on small servers (1v1) or
  reduce lag on busy servers (i.e. Qualifier DM)
* Configurable end of level delay (click or no click) before moving on to
  next map, if a next map is to be loaded at all (i.e., next-level load
  can be halted indefinately if so desired).
* Configurable damage on a per-weapon basis (railgun-only at this time).
* Runs on the following systems/archtectures:
	- FreeBSD-Intel
	- Linux-Alpha-glibc
	- Linux-Intel-glibc
	- Linux-Intel-libc5
	- Solaris-Sparc
	- Solaris-x86
	- Win32-Intel

The rest of this README will describe various aspects of setting
up/running/managing a tourney server.


Server setup
============
The tourney distribution comes with several pre-configured server
configuration files, described below:
	- Single Player DeathMatch with Gladiator bots (SinglePlayerDM.cfg)
	- Single Player Teamplay with Gladiator bots (SinglePlayerTeam3v3.cfg)
	- Single Player 1v1 with a Gladiator bot (SinglePlayer1v1.cfg)
	- Regular DeathMatch (dmonly.cfg)
	- Qualifier DeathMatch (qualifier.cfg) <-- Interesting for tourneys
	- Teamplay Mode (team.cfg)
	- 1v1 Mode (1v1.cfg)
	- Lithium-style Deathmatch (lithium.cfg)
	- Insta-Gib Railgun-only (instadm.cfg)
	- Insta-Gib Railgun Teamplay (instateam.cfg)
						  [I kinda like railgun, eh? :)]

The last three configs are really just examples of the configurability
of OSP Tourney.  They highlight the types of options that are available
to the admin for setting up a server in whatever format they see fit.

Starting a Tourney server:
--------------------------
1. Select the appropriate distribution for your OS/architecture (e.g.,
   Linux/Alpha) and unpack it your your base QuakeII (./Quake2) directory.
   You should have a new "tourney/" directory in your Quake2 directory.
2. Choose one of the above .cfg files appropriate for the server you want
   to run.
3. Decide if you want a listen (where you can play in a Single Player mode)
   or dedicated (stand-alone with no graphic console) server.
4. Edit the rest of the server configuration file to tailor your needs
   (rcon passwords, public availability, server name, etc.)
5. Start the server:
	(For "dedicated" servers)
	quake2.exe +set dedicated 1 +set game tourney +exec <your_config_file>

	(For "listen" servers - i.e. Single Player mode)
	quake2.exe +set dedicated 0 +set game tourney +exec <your_config_file>

	---> NOTE: It is REQUIRED that you have a "+set deathmatch 1" either
		     on the above command line or in your tourney .cfg.
6. Have fun!

====> If you are running under the Win32-x86 platform, you can optionally
	use the Tourney Config Editor (click on the "Tourney Launcher"
	shortcut in the tourney/ directory) to launch either a dedicated
	or listen server.  Help is included for each of the config options
	in an easy-to-use dialogue window.

For client and server command summary and usage, please refer to the
"commands.txt" and "server-settings.txt" files contained in the base
tourney/ directory.

**** PLEASE REFER TO THE "commands.txt" AND "server-settings.txt" FILES FOR
ALL RELEVANT INFORMATION PERTAINING TO COMMANDS (SERVER/CLIENT) AVAILABLE
IN OSP TOURNEY DM *AS WELL AS* SPECIAL CLIENT-SIDE VARIABLES THAT ALLOW FOR
UNIQUE CLIENT CUSTOMIZATIONS ****


The "Heads Up Display" (HUD)
============================
One of the central pieces added to enhance gameplay for players is the
information displayed to the player while they are in the game.  Referred
to as the HUD, it gives information pertaining to the state of the game.
The default id DM mode shipped with QuakeII displayed only the player's
frag count in camoflouged letters in the upper-right corner of the screen. 

Tourney has revamped this information considerably to allow the player to
know immediately how they stand throughout the course of the game.  That
is, they do not need to look at the scoreboard or have previous knowledge
of server settings or even ask other fellow players to determine the
critical aspects of the current match.

All attempts have been made to use a color-coding scheme to allow player's
to quickly identify certain information without having to actually read
and/or think about key values displayed on their HUD.

It is important to note that this HUD is different for each mode of Tourney.
Below, each mode's HUD is described:

Regular DeathMatch:
-------------------
A typical client would see the following in the upper right-hand portion of
their screen while they are in the game:

	Score
	12/40
	-   6

	 Rank
	  4/9

	 Time
	 9:26

Score (1st line):
  Gives the player's frag count out of the fraglimit.  In this example,
  the player has 12 frags and the match fraglimit is 40.  If there were
  no fraglimit on the server, then only the player's frag count would be
  displayed (in this case, only the number "12" would be displayed).

Score (2nd line):
  Displays the number of frags needed to move into first place.  In this
  example, this means that the first place player has 6 more frags (18
  total) than the current player.  If the player was in first place, then
  this count is the number of frags he/she is ahead of the second place
  player.  This second-line number has two formats:
	- White lettering with a "+" in front of the number is for the
	  player in first place.
	- Green lettering with a "-" in front of the number is for all
	  other players not in first.

Rank:
  Tells the player currently what position they reside at any given
  instant in the match.  It is dynamically updated if their rank changes
  or as people enter/leave the match.  In this example, we see that the
  player is currently in 4th place out of a field of 9 active players.

Time:
  Displays the time remaining in the match before the time limit is
  reached and the match is stopped.  This clock starts from the size of
  the timelimit and counts down in semi-real-time.  In this example,
  there is 9 minutes and 26 seconds left in the match, if the fraglimit
  is not reached first.  If there is no timelimit for the match, this
  counter will display "OFF" during the course of the match.  Also, the
  countdown time will flash (between white and green) in the last minute
  of the match (to give a sense of urgency to lower ranked players :-) )
  Finally, it should be noted that the countdown timer can be adjusted to
  appear in the bottom middle of the client's HUD.


Qualifier DeathMatch:
---------------------
While the HUD of the Qualifier Deathmatch mode is similar to the Regular
DM HUD, there is one critical aspect that tells the player whether or not
they are currently "qualified".

The information displayed on the second line of the Score, rather than
being an indicator of the number of frags from 1st place, is a
differential from being above/below the last pre-specified qualifying
spot.

For example, if a player sees the following on their HUD (in the Score
field):

	Score
	   45
	+   8

Then they would know that they are 8 frags above the lowest qualifying
slot (i.e. they are currently qualified).  Again, if they have a "+" sign
here, the text would be white.  A "-" sign would be included with green
text.

In the above example, if the player had green "-   8" below their score,
this would signify that they were 8 frags BELOW the lowest qualifier:
they would not currently be in "qualified" status.

It should be noted that if there are equal to or fewer number of players
than there are qualifying spots, then ALL players would be considered
qualified and ALL players would have a white second line with a "+" in
front the the score differential (based from zero).

All other aspects of the HUD in the Qualifier DM mode are the same as
those specified in the Regular DM mode.


Team Play
---------
The HUD for Teamplay is quite simple.  It just shows team scores and time
remaining in the match:

	FlyGurls
	 (11) 32

	HomeBoyz
	      23

	    Time
	   11:52

The player will see his/her team displayed in white lettering with the
opponent's team in green lettering.  The team score score shown is a
breakdown of the player's score (in "()") and the overall team score (to
the right of the player score).  The team score is the cumulative score
of all members of each team.  There is also a "32/50" variation as shown
in the Regular DM mode if there is a fraglimit ("50" fraglimit in
mentioned example).

Time countdown will again flash in the last minute of the match.

If the match is paused due to a team timeout, the "Time" entry on the HUD
will appear as follows:

	 Time
	TO 23

Where "TO" signifies a time-out is in progress and that (currently)
there is 23 seconds remaining in the time-out.  This number will count
down until the time-out expires.

If a referee pauses the match, then the entry under the "Time" field
will appear as "Pause".  This will remain (i.e. no countdown) until the
referee unpauses the match, or the match is terminated.


1v1 Play
--------
The 1v1 HUD is the same as the Team Play HUD save that the team name and
score are replaced with the player's personal name and score.


Variations of the HUD for Time-Synced modes:
--------------------------------------------
The HUD is dynmically modified for different stages in the time-synced
mode of play (i.e. waiting for players to "ready" themselves, countdown
to match start, and in-game).

Qualifier DM, Teamplay, and 1v1 are all time-synced modes of play.

** WARMUP MODE **
During warmup, all players' scores (teams' scores in teamplay) are
displayed as "WARMUP" while the game waits for everyone to become "ready".
All rank, score differential and time information are not shown.

In Qualifier DM and 1v1 Mode, if a player is ready, there will be a "*"
in front of this warmup message to signal that they have already readied
up (in case they forget :) ).

In teamplay, a player's team will have four distinct tags:
	- None: No member is ready
	- "-": Other team members are ready, but the player is not.
	- "+": The player is ready, but there is at least one other team
		 member who is not ready.
	- "*": All team members are ready.

** COUNTDOWN MODE **
When all players are ready, the match countdown begins with a matchinfo
screen and a modified HUD.

Player scores are replaced with "STARTING" and the Time field pops up
with a countdown timer displaying how much time is left before the match
begins.

This format is the same for all modes of time-synced play.



Modes of Play
=============

Regular DM:
-----------
Runs just like normal id DeathMatch: players just join into the fray and
frag away.  By default, all ammo, weapons, capacities are as they are
found in vanilla DM.  The server op can opt to change these values, add
in the hook, ban items, have different player equipment upon respawn,
etc.

The main difference is the new HUD, new scoreboard, and menuing/vote
system.  There is also random end-level music while reading the
obituaries.

Synced-match modes:
-------------------
The other 3 modes require all players to be "ready" before the match
will commence.  During this period, players are in WARMUP mode and spawn
in with all weapons, extra health and full red armor to, well, umm,
warmup.

When there are enough players who have moved to the "ready" status
(through the "ready" console command), all players will be stripped of
their weapons.  The match countdown will not necessarily start, it just
lets players who have not readied up to be aware that there is some
significant number of players who ARE ready and wanting to start the
match.

When there are enough players who have readied up, an information screen
of the match parameters is displayed, and a countdown to the start of the
match begins.

This screen is displayed through the course of the match startup
countdown (30 seconds or whatever countdown value is configured for the
server).  When the countdown reaches 0:00, players are respawned
throughout various respawn points of the current map and the match
begins.

Qualifier DM:
-------------
This mode was designed specifically with player seeding into a 1v1
tournament in mind.  It has the exact same functionality as Regular DM,
save for time-sync, the signifiers of QUALIFIED/NOT QUALIFED on the HUD
(described previously) and "*" notations on the scoreboard for
"qualified" players.

Teamplay:
---------
This mode allows for 2 teams of players to play against one another,
regardless of the size of each team.  There are commands to allow people
to join, switch, or invite other people into an already started match.
Teams can also be locked to prevent new members from joining, or unlocked
to allow more people to join a team.  Teams are also locked after a match
starts (i.e. nobody can join a team in a game that has already begun
unless they are "invited").

When teams are initially formed, the first player on each team is also
deemed as the "captain" of the team.  The team captain has special
control over his team members and can also perform actions on behalf of
his team.  Referee-based commands deal with readying up his/her entire
team, removing players from the team, calling game timeouts during match
play, or transferring captain-ship to another teammate, if so desired.

The scoreboard displays the winning team on top and dumps team statistics
to the console at the completion of the match.

There are two new terms used in specifying certain kinds of team statistics:
	- Fratricides (or Frats): # of times a team member killed another
	  team member.
	- Blunders: Total sum of frats and suicides of all team members on
	  a team.

These new terms have been added to help teams know exactly what type of
problem is eating into their perceived frag counts.  We found it a very
informative in determining if a team has more problems in suicides or
fratratcides.  These are very handy statistics!

At the end of the match, the winning team gets the cool xian music and
the losers get a Makron laugh in their faces. ;)

1v1:
----
The format of the 1v1 play is similar to that of teamplay, except that
there are no team commands available (max team size is 1 in this regard)
and that "Blunders" and "Frats" are not applicable.  There is also no
concept of a captain in 1v1 mode, although players can still individually
call timeouts if so needed.

All other aspects (scoreboard, stat console dump, music) are the same as
its teamplay counterpart.


"ngStats"
=========
By default OSP Tourney DM logs all game play information to a file for
each map played and then calls ngStats after each map is completed.
ngStats then in the background reads the log file and creates new html
that shows the scoring information for that game in addition to a
detailed statistics breakdown for it. This detailed information is kept
for a number of the most recent games played, before being overwritten
by new games.  See the below for details on how to increase the maximum
stored at one time.

In addition ngStats maintains a running "Career Totals" database that
keeps track of a totals summary of all statistics from every game logged.
If you run LAN parties and would like to learn how to use ngStats to keep
track of the total stats for a LAN party then visit the ngStats section,
listed above, at NetGames USA for details.

The easiest way to view the ngStats information about your game play is
to make a shortcut to the ngStatsQ2T.exe on your desktop and just double
click on it when you want to view your ngStats.  ngStats will then run,
check for any existing log files and then automatically launch your
default web browser to the main page. The ngStatsQ2T.exe is located in
the NetGamesUSA.com/ngStats/ directory that was installed in your Quake2
directory.

ngStats is targeted to a couple of main audiences:
	- The single player environment where a player sets up and plays
	  in matches against Gladiator bots.  ngStats will allow the player
	  to track his/her playing statistics and how they stack up against
	  the bots themselves.  It has proven to be an invaluable aid to
	  players in tracking their deficiencies and strengths.
	- Multi-player enviroments where dedicated servers are set up with
	  multiple clients connecting to play.  With the in-game ngStats
	  processing back-end, players can immediately view their online
	  stats for that particular server.  It will also let players see
	  how they stack up against the other "locals" of the server.

When using ngStats in a single player environment (i.e. running a graphic
"listen" server), the player will have the option to view his/her ngStats
information at any point in the game.  This option can be selected on the
main menu of Tourney by activating the "View ngStats (browser)" option.

When this option is selected, the current game will automatically pause,
launch the default web browser, and display the entire ngStats player
history database in a convenient web page format.  This information is
also displayed (by default) when the player shuts down Quake2.

This in-game viewing is ONLY available to the operator of a listen
server.  It is *not* available to any human connected clients or
operators of dedicated (text console-only) servers.
	  
For additional information, please refer to:
	http://www.netgamesusa.com/ngStats/Quake2
or check the installed documentation found in your NetGamesUSA.com/ngStats
directory (under your main Quake2/ directory).


"ngWorldStats"
==============
ngWorldStats™ is the online version of ngStats™ and default support for
it is built right into OSP Tourney DM! It is a freely available service
where you can get complete and total statistics from your game play on
every participating OSP Tourney DM game server while playing online.

Your every kill, your every death, and even your every Health Pack use
will be remotely logged to NetGames USA for analysis and and post game
presentation. You will then be able to visit the Quake II™ ngWorldStats
site at:

	http://Quake2.ngWorldStats.com/

and see a detailed breakdown of up to the last week's worth of games you
played while online.

Plus all of your stats will also be accumulated into a permanent career
totals database just for you, keeping track of exactly how many frags,
kills, deaths and much more that you have ever accumulated while playing
online.

For additional information, please refer to:
	http://Quake2.ngWorldStats.com/FAQ
or check the installed documentation found in your NetGamesUSA.com/ngStats
directory (under your main Quake2/ directory).


Gladiator Bots
==============
******
NOTE: OSP TOURNEY DM 2.0 COMES WITH q2dm1.aas - q2dm8.aas ALREADY
	CONVERTED AND COMPUTED.  YOU WILL HAVE TO CREATE ADDITIONAL	.aas
	FILES FOR ALL OF THE OTHER MAPS YOU WISH TO HAVE RUN ON YOUR
	SERVER AND HAVE USED WITH THE GLADIATOR BOTS.  TO CREATE THESE
	ADDITIONAL .aas FILES, USE THE INCLUDED winbspc.exe UTILITY (WIN32
	ONLY) OR THE COMMAND-LINE BASED bspci386 (LINUX) UTILITY.  THERE
	ARE SOME ADDITIONAL PRECOMPUTED .aas FILES AT THE OSP SITE:
	http://www.planetquake.com/osp/tourney
******

******
NOTE: AS OF VERSION 2.0 OF OSP TOURNEY DM, GLADIATOR BOTS ARE *NOT*
      AVAILABLE FOR THE LINUX-ALPHA OR SOLARIS-SPARC PLATFORMS.
******
The Gladiator Bots that are included with OSP Tourney DM are server-side
bots developed exclusively by MrElusive.  Since they are server-side bots,
they do not require any real (human) player intervention and act on their
own accord.  However, they can be "commanded" by fellow human teammates
under certain modes of Tourney.

The Gladiator bots are available for all modes of Tourney (Regular DM,
Qualifier DM, Teamplay, and 1v1).  These bots understand the different
modes and act accordingly in each style of play (auto-joining, readying
up during warmup, selecting teams to join, not shooting fellow teammates,
etc.).  A lot of work has been put into setting up and playing with these
bots under each specific mode of Tourney.

For clients, all interaction in adding or removing bots is through the
voting system, if it is enabled.  Clients can also use the "players"
command to identify which players are real and which are Gladiator bots
(denoted by the "[BOT]" tag next to their name in the "players" listing).

For the server admin, several things are required to properly set up
gladiator bots on their servers:
	- Configure all bots_* commands as found in the "server-settings.txt"
	  configuration reference file.
	- Create .aas files for all maps that will be used on the server.

The reference bots*.cfg files have been heavily used on my public servers
and have been shown to be fairly balanced for most types of gameplay.
If you want to create new bot characters or modify existing ones, please
refer to the extensive documentation included in the gladiator/ directory
of the Tourney distribution.

For all of the different ways in which the bots can be loaded into a
server again, refer to the "server-settings.txt" file for a description
of the bots_* commands and their settings.  Such things as auto-loading
of bots based on server player-counts, static loads, etc. are available.

The .aas files are restructured versions of the map files (.bsp) used for
Quake2.  These .aas files are needed by the Gladiator bots to properly
navigate maps under Quake2.  Failure to create a .aas for a map when a bot
is loaded will cause a deliberate server crash.

To create a .aas file, use the included winbspc.exe (Win32 only) to find
the maps you want converted (in .pak files or in maps/ directory), and
place these conversions in the directory of your choice.  Don't be
alarmed by the initial size of these .aas files.  They contain surplus
information for the Gladiator library to examine and decide which
elements are important, and which are not.

After the initial .aas file has been created, it must be further
processed by the Gladiator library to calculate relevent bot
reachabilites and to throw out spurious (and useless) information.
This step need only be done once as the Gladiator library, after
computing these reachabilites, resaves this information into the .aas
file, often reducing its size by at least half.

To perform these initial reachability calculations, it is *REQUIRED*
that the server admin first load up a listen server, and vote in a bot.
Once the bot is loaded in, the server will become quite laggy and give a
text-based progress report of its reachability calculations.  Once the
calculations are complete, the .aas is now ready for use in multi-player
environments.  It is *highly* recommended that no other clients be
allowed to connect while this initial reachability calculation is in
progress, as it can cause an unexpected server crash.

The main point here is that a freshly created .aas file (from
winbspc.exe) should NOT be put on a main server for bots.  The secondary
preprocessing step, while inconvenient, ensures that the Gladiator
library has only the information it needs to properly navigate bots
through the current map.

It should be noted that only the server administrator needs to perform
these steps.  Connecting clients do not need to do anything to play on
a server that has Gladiator bots enabled.

For additional information and the latest revisions of the Gladiator bot
library and its auxillary utilities, check:

	http://www.botepidemic.com/gladiator


OSP Tourney DM Configuration Editor
===================================
The auxillary server configuration editor to handle and configure ALL of
OSP Tourney's various settings can be found in the ConfigEditor/
directory in the tourney/ directory.  Just click on the "TourneyCE.exe"
and your all set.  Inline help is also included with this utility.

It is recommended that for players who will predominately play Tourney
for its Gladiator bot support and wish to start their own Quake2 "listen"
server (graphic mode) for single-player modes, the Configuration Editor
should be used to start up OSP Tourney DM.

With its capability to load up older or preconfigured configuration
scripts this is a safe and easy way to get a server rolling for anyone who
is not an expert on setting up a Quake2 server for OSP Tourney (or how to
configure Tourney for that matter :) )


High Scoring Facility
=====================
OSP Tourney DM has the capability to keep track of the top 10 scores for
all maps played on a server.  It should be noted beforehand that this
feature is ONLY available under Tourney's FFA (Regular DM) mode.

This system is an auto-configuring setup that will create directories
and score histories as needed.  When first enabled, it will create a
highscores/ dir under your main tourney/ directory.  It then creates
new directories based on the port # of the server running the high
scores.  Below this, each map will have its own unique high score
history file.

Player ranking is based on the setup of the server:
	- Finite fraglimit:  Ranking based on FPH (Frags Per Hour)
	- No fraglimit, finite timelimit: Ranking based on Frag count.
	- No fraglimit, no timelimit: No high scores kept.
Also, only the WINNER of the current round is eligable to be placed in
the high scores list, if they rank high enough.  Thus, this high scores
list is really a WINNERs' high score list.

There are several features of the high scoring facility to consider:
	- High scores will automatically show up approximately 9 sec
	  after viewing the scoreboard.  After 9 sec of viewing
	  highscores, it will go back to normal scoreboard.  It will
	  continue to cycle in this fashion.  So, as a note to server
	  admins, setting nextlevel_click and nextlevel_default vars
	  to something greater than 20 sec will allow clients to get
	  a full view of both the scoreboard and the current high score
	  list for the given map, when a match is complete (i.e., end-
	  of-level scoreboard viewing).
	- Also, if the timelimit/fraglimit of the server changes, the
	  highscore lists WILL be reset to reflect the currect server
	  parameter's player rankings.
	- Remember, this ONLY works for regdm play (match_mode = 0).
        It does NOT work in any other mode of play.


Client Server Config Voting
===========================
Clients also have the ability to vote for specific (and preselected)
server configurations on the server.  This feature is much more than
just switch from teamplay to FFA.  It allows server aministrators to
have multiple configurations available on their server for clients to
choose.

For instance, it is now possible for a single server to host site to
normal 1V1 DM, teamplay, rail-only FFA, regular DM with an auto-loading
of 10 bots feature, clan server for practice with private passwords, etc.
All of this can be made to clients through a straight-forward setup
script.  Whatever the admin wants to be made available for clients is
now possible.

The only real setup is to create all of the alternate server
configuration files (format is described below), and to create an
external list of these configurations.  This list is then used to
advertise to clients on what alternate configurations can be used.

Points to consider (for the administrator):

	- The format of these alternate config files should be
	  nearly identical to those used to start a Tourney
	  server.  The *only* requirement is to remove any
	  "map <mapname>" entries from the file so as to
	  prevent a double-map load when a new config file is
	  voted in.  Even if you forget, the worst that can
	  happen is that clients will have to wait for 2 back-to-
	  back loads of a map before they can begin play.
	- The list of the alternate server configuration files that
	  should be made available (specified in the server variable:
	  "set vote_config_list <filename>") is a single column text
	  listing of the alternate server config filenames that
	  clients can vote upon.

There is also the option to specify a "default" configuration file that
can be used to reset the server after some period of inactivity after
an alternate server configuration has been voted in.  Check the
"server-settings.txt" document for further details.


Lithium-style Runes
===================
As of version 1.75 of OSP Tourney DM, the capability to add "rune"
objects in the game is available.  These runes behave *exactly* like the
runes found in the popular Lithium Quake2 mod (except for the
regeneration rune, as noted below).

For those who are not familiar with Lithium-style runes, the list below
describes the attributes of each rune:

	- "Resist" Rune (BLUE):  Players take half damage.
	- "Strength" Rune (RED): Players give double damage.
	- "Haste" Rune (YELLOW): Players fire weapons at twice their normal
	  rate.
	- "Regen" Rune (GREEN): Players regenerate 3 points of health every
	  0.8 seconds, up to 200 health points.  If a player has armor, it
	  regenerates 3 points of armor every 0.8 seconds, up to 100 armor.
	  If a player is regenerating both health and armor at the same
	  time, the updates occur every 1.6 seconds.
	- "Vampire" Rune (PURPLE): Players get half of the damage they give
	  to other players, up to a maximum health of 200.

Tourney also has the capability to configure the models used for the
runes (with the default being John Carmack's (the main author of Quake2)
head :) ).  Further, Tourney can be configured to have the player flash
the color of their rune when it is used (i.e., players flash RED when
shooting their weapons (stength rune), blink GREEN when they regenerate
health/armor (regen rune), etc.).

The name of the current rune a player is holding is displayed on the
right-side of the player's HUD, just below the time-remaining countdown.
If a player is not holding a rune, there will be nothing displayed in
this portion of the HUD.


Use of OSP Tourney DM in Tournament Environments
================================================
Most of the configuration files included with this distribution are ready
to go for most LAN tournament events (i.e. qualifier.cfg and 1v1.cfg).
However, there are several options that can be enabled or changed to
facilitate some of the different needs of the tournament organizers
and/or players.

- Client-side screenshots and demo recording:
	- Enabling the demo_player server variable forces clients to take
	  demos of their Qualifier DM, Teamplay, or 1V1 matches.  It will
	  also force an end of level screenshot of the scoreboard for each
	  client.

	  The demo_referee forces this on referees within the game as well
	  (i.e. for chasecam views of players).  This option is *not*
	  available for Regular DM mode matches.
	  
	  All collected demos for the client are stored in tourney/demos.
	  All collected screenshots for the client are stored in
	  tourney/scrnshot.  Note, these files are stored on the CLIENT's
	  machine, not the server.

	- The naming convention for client-side demos is as follows:
		- Qualifier DM:
			 <player_name>-<demo_tag>-<map>-<time>.dm2

		- Teamplay:
			<player>-<pl's_team>-<opp_team>-<demo_tag>-<map>-<time>.dm2

		- 1V1:
			<player>-<opponent>-<demo_tag>-<map>-<time>.dm2

		---> Referee filenaming is same as above, except with a
		     "REF-" prepended to the filename.
		---> <demo_tag> refers to the demo_tag variable in the server
		    .cfg file.
		---> All player names, team names, and demo_tag strings are
		     stripped of any illegal characters that cannot be used
		     in a filename before the demo is created.  This is just
		     FYI.

	- There is no automatic method under Quake 2 to retrieve these
	  client-side demos and screenshots.  They will have to be manually
	  collected from clients after their games are complete.

- Other server parameters to consider:
  The following paramter list are probably the most critical in
  configuring an OSP Tourney DM server for match play during
  competitions:
	- numgibs  (spectacular demos (1v1) vs. performance (24-player
	  qualifier on q2dm8) --> set this value high (6-8) for 1v1, set it
	  low (0-1) for Qualifier FFA.
	- allow_id  (allowing opponent ID tagging is questionable during a
	  competition)
	- respawn_delay  (amount of time a client HAS to reenter after a
	  death (wipe-thy-hands-and-get-ready-to-enter-again time))
	- referee_* commands (allow the use of referees in a match)
	- motd_file (special message for players)
	- nglog_* / sl_* logging (ngTCS, ngStats, ngWorldStats, GibStats,
	  etc.)
	- match_latejoin (if others can join while a match is in progress..
	  performance issues)
	- vote_enable_* (if settings are preset and should not be changable
	  by clients)
	- demo_* (to enable client-side demos and naming of demos during
	  match play)
	- map_halt (to indefinately pause a server after match is over:
	  screenshots, etc.)
	- match_strictmode (whether clients can be allowed to "ready-up" and
	  start a match, or the referee has total control on match start
	  through the use of r_allready/r_allnotready commands)

- Client allow/deny features:
  It is also useful to consider using the IP and playername deny
  capabilities of Tourney to allow ONLY players who are supposed to play
  in a match on a particular server.  Of course, it is helpful if you
  have an automated system such as the NetGames Tourney Control System
  (ngTCS) that can set these types of options dynamically.  You can take
  a look at the ngTCS at:
	http://www.netgamesusa.com/ngTCS

  ----> Please refer to the included "server-settings.txt" file for a
	  complete description on all of the above-mentioned options and
	  their default settings.

- Utilize the ngStats logging system and utilities (just a config option
  away) to automatically update all player information on all matches
  that have been played.


Miscellaneous Information
=========================
- Advanced teamplay users can take advantage of the special client-side
  variables that are recognized by Tourney to avoid having to set up
  their team properly at the start of every new match.  This feature is
  especially useful for organized clan matches where the teams do not
  change (i.e., name, skins, joincodes) and everthing must remain
  consistant.  All clan members can add the following variables to their
  Q2 startup scripts to ensure consistant settings during their clan's
  matches:
	- default_teamname
	- default_teamskin
	- default_joincode

- For those not familar with the CTF-style "%" macros, the following is
  a description of the macros that can be used in teamplay to communicate
  ongoing match information to fellow teammates:
	- %l/%L : Gives speaking player's location on map.
	- %a/%A : Tells the amount of armor the speaking player's currently
		    possesses.
	- %h/%H : Tells the amount of health the speaking player's currently
		    possesses.
	- %w/%W : Tells the current weapon the speaking player is currently
		    holding.
	- %n/%N : Announces the closest player to the speaking player.
	- %r/%R : Specifies the current rune held by the speaking player.
	- %t/%T : Specifies the current rune held by the speaking player.
  This info is only given if the player uses the above-mentioned sequences
  with the "say_team" or "steam" commands when the mod is in "Teamplay"
  mode.

- There is a special "trick" on setting up maps in the map queue so as to
  have a core set of maps that are always cycled through during regular
  play, but to also have special custom maps that are available for
  voting.  The idea here is that a server admin may want only the regular
  Quake2 maps on the standard rotation so that connecting clients dont
  have to worry about (or download) custom maps that may not be very
  popular, yet have these custom maps available for players who do have
  the maps and want a change of pace.

  To set up the map rotation in this regard, simply set the min_players
  setting on the custom or special maps to be HIGHER than the maxclients
  setting for the server.  This ensures that the maps are NEVER cycled
  to in the course of a normal map rotation, but they ARE available for
  voting.  Simple, eh?  :)

- This version of tourney has a new logging system (in addition to the
  Standard Log) called ngLog.  ngLog provides a much richer event logging
  facility that gives more information on actions and events that occur
  within the game.  Please visit:  http://www.netgamesusa.com/ngLog for
  complete information on this logging system.

  A component that has also been included with ngLog is ngWorldStats.
  ngWorldStats is a Internet stat tracking system that keeps track of ALL
  games (and players) that are played on servers and have ngWorldStats
  logging enabled.  Clients simply need to have the client variable
  "ngWorldStats_password" set to some string of their choosing (i.e.
  somewhere in a client's autoexec.cfg):

		 set ngWorldStats_password my_special_password_string

  The "my_special_password_string" is any sequence of characters and can
  be any length in size.  It is *highly* recommended to use a string of
  8 characters, but this is not a requirement.  ngWorldStats identifiers
  are based on both the player's name and ngWorldStats_password string.
  Thus, if a client changes either their name or ngWorldStats_password
  string, then they will have a different unique player within the
  ngWorldStats database.

  The strength of ngWorldStats is that it keeps a LOT of information on
  each player and loses no information whatsoever on servers that are
  ngWorldStats-enabled.  This approach is quite different than the
  current game server polling statistical methods in place today that can
  miss a lot of frag counts on quick matches.

  Server operators can participate in this Internet-wide player/server
  statistics gathering by simply enabling ngWorldStats within tourney:
    set nglog_worldstats 1
  Note, enabling ngWorldStats requires *very* little processing overhead
  and does not add any noticable load to the server itself, as it runs
  independant of the main Q2 game thread.

  Note, if the server is going to be in a closed environment (i.e. not
  connected to the Internet) or there are other network-related issues
  involved with using ngWorldStats, servers operators should:
  	"set nglog_worldstats 0"
  in their server configuration file.

  Players can determine if a server is ngWorldStats-enabled by looking at
  the severinfo variable "ngWorldStats_Status" on game browser utilities
  such as GameSpy.  If the value of this variable is "*** ENABLED ***",
  then all events will be sent to the main ngWorldStats collector sites.
  A value of "<<< Disabled >>>" means that no player stat information is
  being recorded to the global ngWorldStats system.

  There is also a GibStats-like log file parser that can interpret local
  ngLog logfiles.  This utility is called "ngStats" and more information
  regarding its features and capabilities can be found at:
	http://www.netgamesusa.com/ngStats

  Mod authors interested in implementing ngLog/ngWorldStats in their mods
  for global statistics collection can visit:
	http://www.netgamesusa.com/ngWorldStats
  or send mail to: ngDev@netgamesusa.com

- For the latest info, discussion, and utlities for the Gladiator Bots,
  check the homesite for them at:

	http://www.botepidemic.com/gladiator

  This site also has the latest revisions of the Gladiator Bot libraries
  for all architectures and should be plug-and-play compatible with
  Tourney.  To update to the latest Gladiator library, just download the
  full Gladiator distribution and extract ONLY the
  gladiator.dll/gladi386.so and pak7.pak to your tourney/ directory.

- If you don't want to have 100's of .aas files laying around (reference
  files for the Gladiator bots for each map), you can add them to any
  zipfile named aas?.zip (? = 0,1,2,..,9).  Note, do NOT add .aas files
  that have not yet had their reachabilities calculated.  Adding the .aas
  files to a .zip takes a marginally longer time to load the first bot,
  but it cleans up your base tourney/ directory and cuts most .aas files
  down by half in size.

- If your having excessive client overflows at the start of semi-large
  teamplay matches (4v4 or more), try setting match_countinfo to 0.
  This will eliminate the start-of-match information screen, but should
  help to curb aggrevating client overflows.

- Quake2 has a nasty, seemingly random bug that is related to taking
  screenshots. If clients are having crashes when taking screenshots while
  playing Tourney, then they need to make a scrnshot/ directory in their
  tourney/ directory.  Not all players seem to be affected by this bug, as
  it only occurs if the scrnshot/ or tourney/ directory does not exist
  when attempting to take a screenshot.  Once this directory exists,
  screenshot client crashes should not occur.

- The dimensions of the MOTD window is 9 rows of characters by 32 columns
  of characters. All of the MOTDs included with this distribution use the
  MOTD Edit utility by Tony Browneller.  You can grab the latest version
  of this handy utility from: http://qnm.telefragged.com.  Thanks Tony!

- On some systems, selecting the "View ngStats (browser)" option while
  running a listen server will not refresh the Quake2 display if it is
  reselected after viewing stats.  A predominant characteristic of this
  problem is that clients cannot properly Alt-TAB out of their Quake2
  window under any circumstance and under any Quake2 mod.

  If you are having problems in this regard, you can try the following
  setting in your server config file:

	set nglog_ngstats_vidrestart 1

  This setting appears to work in a large majority of the cases.
  However, depending on what action is taking place around you when
  refocus the Quake2 window, the server may crash with a "BAD MODELTYPE"
  error.
 
  If you're in a bind, another possible work-around is to blindly type:
	vid_restart
  in the console (you may have to also blindly hit the "~" to get the
  console before entering the command as well) when the Quake2 window
  has focus.  Not sure what the problem is here exactly.  In any case,
  I am sure it is a video driver issue and not Tourney's 8)

  There are some cases where even the Quake2 video will not switch
  out when this option is selected.  In one case, this has been found
  to be related to the miniGL driver (version 1.47) for voodoo2 (SLI,
  possibly).  Switching back to version 1.46 of the miniGL driver
  appears to address this problem.

- The use of front-end server utilities such as "bw-admin" can cause
  problems in creating directories and information files for the high
  score facility.  A work-around that has been shown to be effective is
  to create the "highscores/" and "highscores/<server_port_#>"
  directories by hand before starting a Tourney server.

- Some admins have experienced problems with the message "info string
  length exceeded" repeatedly showing up shortly after starting their
  tourney server.  The reason for this error has been the fact that they
  were setting multiple public server info variables that were rather
  long in length.  So, if you are having this problem, the probable
  culprit(s) are your local "set <var_name> <information> s" variables.
  Comment out the variables until the error stops.  Then, either reduce
  the information in these variables or remove some of them completely.

  Another problem that can result for public server vars are strange
  interactions with map loads.  There are probably other stange
  artifacts that are a result of having long public variables, so be
  sure to check this if you have recently added some of these variables.

  BTW, server info variables are those variables which are used to
  describe information for a particular server and viewable by such
  utilities as GameSpy.  Typically, they give Email/ Web, Connection
  information and can be created by simply stating:
     set <any_string_name> <any_info_content> s
  (Note the trailing "s" in the set command, as it specifies to Quake2
  that it is a public info server variable).


Known issues
============
- None known at 2.5 release.


Mod Information
===============
This mod was developed from scratch starting in Nov'98.  I developed it
from the vanilla 3.14 DM source and worked like mad (outside of my job
and family) to get it where it is today.  Many of the elements found in
this mod were taken from features touched upon in several other mods.  I
would say that this mod really is a collection of the best features found
in these fabulous Q2 mods: Lithium, Battle, GX, and Duel.  Of course, I
had to put the proper spin on them all to get them *exactly* right in the
context of tournament play.  I guess I added a few new features as well
:-)

A big shout out to my OSP partner-in-crime BotanikA who was a slave
driver :) and more anal than me (believe it or not) in getting things
exactly right.  I owe a lot of stuff in this mod to you man, thanks a
bunch.

Also gotta say "Thanks Fellas!" to the whole ITTC/KU crew:  Lord Drimacus,
Barktooth, Locutis, Dovienya, Gawain, El Presidente, Vitamins, Iceweasel,
SillyK0ne (silky :)) Parallax and the KU dorm peeps.  You all helped out
so much in getting this thing to work with more than one person running
around just shooting at the walls (me).

"I love you man!" goes out to Mr. Elusive and his phenomenal Gladiator
bot.  It is the best bot for Quake2, hands down.  Check it out at
	http://www.botepidemic.com/gladiator.
Thanks for all the help, Elu :)

A deep bow to Luis Filip Toscano for his outstanding OSP Tourney
Configuration Editor.  Now even I can configure Tourney :)  For the
latest version and information on this fabulous utility, check out:
	http://homepage.esoterica.pt/~smashing/TourneyCE/

Thanks go to Paul Jordan and his reference implementation of the AutoCam.

Shout out to some peeps who helped steer me in the right direction on a
lot of the teamplay and admin control facilities: Nima, Rune, a|wiseguy
and the whole Abuse crew for play-testing, [Mogh].KemosabE, tHAk, Jumper
[BiP], YETi, and Excelsior.

Stand-alone hoorah to Nima for hooking up the #osp channel on ETG net.
Thanks!!!

Another bow to a|wiseguy for the various tinkerings to teamplay that make
it oh-so-much better :)  Thanks man!

Big-warm-wet-lickery-kiss to all of the NetGamesUSA peeps in getting the
logging stuff (both local and remote) down pat.  This new logging really
gives you a sense that you actually did something while playing Tourney :)

Extra special thank you goes out to Peter "Riviera" Engstrom.  His help
in bot detection was invaluable.  Thanks man! :)  Peter's email is
<riviera@earthling.net>  Check out his Assault mod at
	http://www.captured.com/assault/

Of course, gotta take a bow to the id kings who made this all possible:
"Thanks, Kings!"

Thanks go to Godsmurf.Cx for putting together a nice info HTML on using
Tourney to hold "Lithium Wars" competitions (tourney_as_lithium.html in
the tourney/ directory)

I hope you enjoy playing OSP Tourney DM.  It was a blast to write and I
even honestly think it is a blast to play.  I put in the features that I
know I wanted and that also make the game of Q2 DM so much more fun to
play.


Ta ta,
-Rhea

rhea@orangesmoothie.org
http://www.orangesmoothie.org/
(Alternate site: http://www.planetquake.com/osp/tourney)

[ Lastest revisions of this document and OSP Tourney DM server-side .dlls
  and distributions can be found at: http://www.orangesmoothie.org/

  We also have a channel open for IRC discussions on ETG (EnterTheGame)
  under #osp.  Come join in on discussions on OSP Tourney DM!  Info on
  ETG can be found at:  http://www.enterthegame.com ]
