//===========================================================================
//
// Name:        p_observer.h
// Function:    observer mode
// Programmer:  Mr Elusive (MrElusive@demigod.demon.nl), 1998-01-12
//
// COLOSSEUM: R-EXTRA-6's `dm`/`sp`/`ctf` implementation.  `camera_t` itself is
// in g_local.h, because it is a member of gclient_t and g_save.c has to see it
// to describe it.
//===========================================================================
#ifndef P_OBSERVER_H
#define P_OBSERVER_H

#define CAMFL_NOSMOOTHING       1
#define CAMFL_FIXED             2
#define CAMFL_AUTOCAM           4
#define CAMFL_CHASECAM          8
#define CAMFL_NAME              16

float AngleDifference(float ang1, float ang2);
int DoObserver(edict_t *ent, usercmd_t *ucmd);
void ClientCycleCamera(edict_t *ent);
void ClientSetCamera(edict_t *ent);
void ClientToggleObserver(edict_t *ent);
void ClientToggleAutoCam(edict_t *ent);
void ClientToggleChaseCam(edict_t *ent);
void ClientToggleCameraFixed(edict_t *ent);
void ClientToggleCameraName(edict_t *ent);
void ClientObserverHelp(edict_t *ent);
bool ClientObserverCmd(const char *cmd, edict_t *ent);
void ClientSetViewAngles(edict_t *ent, vec3_t new_angles, vec3_t cmd_angles);
void SetClientView(edict_t *ent, vec3_t end, vec3_t out_angles, vec3_t cmdangles);
void CheckValidCamera(edict_t *ent);

#endif // P_OBSERVER_H
