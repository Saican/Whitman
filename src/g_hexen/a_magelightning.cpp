/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "vm.h"
#include "g_level.h"
*/

#define ZAGSPEED	1.

static FRandom pr_lightningready ("LightningReady");
static FRandom pr_lightningclip ("LightningClip");
static FRandom pr_zap ("LightningZap");
static FRandom pr_zapf ("LightningZapF");
static FRandom pr_hit ("LightningHit");

DECLARE_ACTION(A_LightningClip)
DECLARE_ACTION(A_LightningZap)

//============================================================================
//
// A_LightningReady
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightningReady)
{
	PARAM_ACTION_PROLOGUE(AActor);

	DoReadyWeapon(self);
	if (pr_lightningready() < 160)
	{
		S_Sound (self, CHAN_WEAPON, "MageLightningReady", 1, ATTN_NORM);
	}
	return 0;
}

//============================================================================
//
// A_LightningClip
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightningClip)
{
	PARAM_SELF_PROLOGUE(AActor);

	AActor *cMo;
	AActor *target = NULL;
	int zigZag;

	if (self->flags3 & MF3_FLOORHUGGER)
	{
		if (self->lastenemy == NULL)
		{
			return 0;
		}
		self->SetZ(self->floorz);
		target = self->lastenemy->tracer;
	}
	else if (self->flags3 & MF3_CEILINGHUGGER)
	{
		self->SetZ(self->ceilingz - self->Height);
		target = self->tracer;
	}
	if (self->flags3 & MF3_FLOORHUGGER)
	{ // floor lightning zig-zags, and forces the ceiling lightning to mimic
		cMo = self->lastenemy;
		zigZag = pr_lightningclip();
		if((zigZag > 128 && self->special1 < 2) || self->special1 < -2)
		{
			self->Thrust(self->Angles.Yaw + 90, ZAGSPEED);
			if(cMo)
			{
				cMo->Thrust(self->Angles.Yaw + 90, ZAGSPEED);
			}
			self->special1++;
		}
		else
		{
			self->Thrust(self->Angles.Yaw - 90, ZAGSPEED);
			if(cMo)
			{
				cMo->Thrust(self->Angles.Yaw - 90, ZAGSPEED);
			}
			self->special1--;
		}
	}
	if(target)
	{
		if(target->health <= 0)
		{
			P_ExplodeMissile(self, NULL, NULL);
		}
		else
		{
			self->Angles.Yaw = self->AngleTo(target);
			self->VelFromAngle(self->Speed / 2);
		}
	}
	return 0;
}


//============================================================================
//
// A_LightningZap
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightningZap)
{
	PARAM_SELF_PROLOGUE(AActor);

	PClassActor *lightning = PClass::FindActor(self->GetClass()->MissileName);
	AActor *mo;

	if (lightning == NULL)
	{
		lightning = PClass::FindActor(NAME_LightningZap);
	}

	CALL_ACTION(A_LightningClip, self);

	self->health -= 8;
	if (self->health <= 0)
	{
		self->SetState (self->FindState(NAME_Death));
		return 0;
	}
	double deltaX = (pr_zap() - 128) * self->radius / 256;
	double deltaY = (pr_zap() - 128) * self->radius / 256;
	double deltaZ = (self->flags3 & MF3_FLOORHUGGER) ? 10 : -10;

	mo = Spawn(lightning, self->Vec3Offset(deltaX, deltaY, deltaZ), ALLOW_REPLACE);
	if (mo)
	{
		mo->lastenemy = self;
		mo->Vel.X = self->Vel.X;
		mo->Vel.Y = self->Vel.Y;
		mo->Vel.Z = (self->flags3 & MF3_FLOORHUGGER) ? 20 : -20;
		mo->target = self->target;
	}
	if ((self->flags3 & MF3_FLOORHUGGER) && pr_zapf() < 160)
	{
		S_Sound (self, CHAN_BODY, self->ActiveSound, 1, ATTN_NORM);
	}
	return 0;
}

//============================================================================
//
// A_MLightningAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MLightningAttack)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_CLASS_DEF(floor, AActor);
	PARAM_CLASS_DEF(ceiling, AActor);

	AActor *fmo, *cmo;

	fmo = P_SpawnPlayerMissile (self, floor);
	cmo = P_SpawnPlayerMissile (self, ceiling);
	if (fmo)
	{
		fmo->special1 = 0;
		fmo->lastenemy = cmo;
		CALL_ACTION(A_LightningZap, fmo);	
	}
	if (cmo)
	{
		cmo->tracer = NULL;
		cmo->lastenemy = fmo;
		CALL_ACTION(A_LightningZap, cmo);	
	}
	S_Sound (self, CHAN_BODY, "MageLightningFire", 1, ATTN_NORM);

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			weapon->DepleteAmmo (weapon->bAltFire);
		}
	}
	return 0;
}

//============================================================================
//
// A_ZapMimic
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ZapMimic)
{
	PARAM_SELF_PROLOGUE(AActor);

	AActor *mo;

	mo = self->lastenemy;
	if (mo)
	{
		if (mo->state >= mo->FindState(NAME_Death))
		{
			P_ExplodeMissile (self, NULL, NULL);
		}
		else
		{
			self->Vel.X = mo->Vel.X;
			self->Vel.Y = mo->Vel.Y;
		}
	}
	return 0;
}

//============================================================================
//
// A_LastZap
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LastZap)
{
	PARAM_SELF_PROLOGUE(AActor);
	PClassActor *lightning = PClass::FindActor(self->GetClass()->MissileName);

	AActor *mo;

	if (lightning == NULL)
	{
		lightning = PClass::FindActor(NAME_LightningZap);
	}
	mo = Spawn(lightning, self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->SetState (mo->FindState (NAME_Death));
		mo->Vel.Z = 40;
		mo->SetDamage(0);
	}
	return 0;
}

//============================================================================
//
// A_LightningRemove
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightningRemove)
{
	PARAM_SELF_PROLOGUE(AActor);

	AActor *mo;

	mo = self->lastenemy;
	if (mo)
	{
		mo->lastenemy = NULL;
		P_ExplodeMissile (mo, NULL, NULL);
	}
	return 0;
}
