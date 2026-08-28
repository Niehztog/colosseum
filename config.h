// Colosseum build configuration.
//
// Q2PRO's engine build generates this with meson; a standalone game library
// only needs the switches that shape the vendored headers under inc/.  Same
// role as osp-tourney/config.h and rocketarena2-public/config.h, which is
// where this shape comes from (R-CORE-9).
//
// R-ENG-1  USE_NEW_GAME_API is on BY DEFAULT, so the library targets
//          GAME_API_VERSION_NEW (3302) with gclient_new_t / pmove_new_t, and
//          MAX_STATS is 64 (MAX_STATS_NEW) rather than 32 -- which R-OSP-7
//          clause 6 depends on.  `make API=old` overrides it to 0 for a
//          GAME_API_VERSION_OLD (3) library; see R-ENG-1a and the Makefile.
// R-ENG-2  USE_PROTOCOL_EXTENSIONS is on so game.csr can be the extended
//          configstring remap; whether it is actually used is negotiated at
//          runtime against sv_features, not decided here.
// R-ENG-8  VERSION is "colosseum", replacing the donors' "osp-q2pro" and
//          "ra2-q2pro".  CPUSTRING is derived from the build by the Makefile
//          rather than hardcoded "x86" as both donors leave it.

#define USE_CLIENT              0
#define USE_SERVER              0
#define USE_PROTOCOL_EXTENSIONS 1
#define USE_DEBUG               0
#define USE_FPS                 0
#define USE_MD5                 0
#define USE_LITTLE_ENDIAN       1

// inc/shared/platform.h tests this with #if; define it rather than relying on
// an undefined identifier evaluating to 0, so -Wundef stays usable.
#define USE_GAME_ABI_HACK       0

// R-ENG-1a.  The ONE switch in this file the Makefile may override, so that
// `make API=old` can build the GAME_API_VERSION_OLD library without editing a
// committed file.  Guarded rather than unconditional because -DUSE_NEW_GAME_API=0
// on the command line plus a bare #define here is a redefinition, and R-BUILD-2
// makes that a build failure rather than a warning.  New is the default and the
// shipped configuration: an unqualified `make` is unchanged by this.
#ifndef USE_NEW_GAME_API
#define USE_NEW_GAME_API        1
#endif

#define VERSION                 "colosseum"
#define BUILDSTRING             "portable"

// The Makefile passes -DCPUSTRING per target, using the same mapping the
// engine applies (q2pro/meson.build's cpuremap: x86->i386 on ELF and x86 on
// PE, aarch64->arm64).  The fallback below only fires for a hand-rolled
// compile that forgot to pass it.
#ifndef CPUSTRING
#define CPUSTRING               "unknown"
#endif
