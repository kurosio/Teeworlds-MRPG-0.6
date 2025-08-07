/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

// this file will be included several times

// physics tuning
MACRO_TUNING_PARAM(GroundControlSpeed, ground_control_speed, 10.0f)
MACRO_TUNING_PARAM(GroundControlAccel, ground_control_accel, 100.0f / TicksPerSecond)
MACRO_TUNING_PARAM(GroundFriction, ground_friction, 0.5f)
MACRO_TUNING_PARAM(GroundJumpImpulse, ground_jump_impulse, 13.2f)
MACRO_TUNING_PARAM(AirJumpImpulse, air_jump_impulse, 12.0f)
MACRO_TUNING_PARAM(AirControlSpeed, air_control_speed, 250.0f / TicksPerSecond)
MACRO_TUNING_PARAM(AirControlAccel, air_control_accel, 1.5f)
MACRO_TUNING_PARAM(AirFriction, air_friction, 0.95f)
MACRO_TUNING_PARAM(HookLength, hook_length, 380.0f)
MACRO_TUNING_PARAM(HookFireSpeed, hook_fire_speed, 80.0f)
MACRO_TUNING_PARAM(HookDragAccel, hook_drag_accel, 3.0f)
MACRO_TUNING_PARAM(HookDragSpeed, hook_drag_speed, 15.0f)
MACRO_TUNING_PARAM(Gravity, gravity, 0.5f)

MACRO_TUNING_PARAM(VelrampStart, velramp_start, 550)
MACRO_TUNING_PARAM(VelrampRange, velramp_range, 2000)
MACRO_TUNING_PARAM(VelrampCurvature, velramp_curvature, 1.4f)

// weapon tuning
MACRO_TUNING_PARAM(GunCurvature, gun_curvature, 1.25f)
MACRO_TUNING_PARAM(GunSpeed, gun_speed, 2200.0f)
MACRO_TUNING_PARAM(GunLifetime, gun_lifetime, 2.0f)

MACRO_TUNING_PARAM(ShotgunCurvature, shotgun_curvature, 1.25f)
MACRO_TUNING_PARAM(ShotgunSpeed, shotgun_speed, 2750.0f)
MACRO_TUNING_PARAM(ShotgunSpeeddiff, shotgun_speeddiff, 0.8f)
MACRO_TUNING_PARAM(ShotgunLifetime, shotgun_lifetime, 0.20f)

MACRO_TUNING_PARAM(GrenadeCurvature, grenade_curvature, 7.0f)
MACRO_TUNING_PARAM(GrenadeSpeed, grenade_speed, 1000.0f)
MACRO_TUNING_PARAM(GrenadeLifetime, grenade_lifetime, 2.0f)

MACRO_TUNING_PARAM(LaserReach, laser_reach, 800.0f)
MACRO_TUNING_PARAM(LaserBounceDelay, laser_bounce_delay, 150)
MACRO_TUNING_PARAM(LaserBounceNum, laser_bounce_num, 1)
MACRO_TUNING_PARAM(LaserBounceCost, laser_bounce_cost, 0)

MACRO_TUNING_PARAM(PlayerCollision, player_collision, 1)
MACRO_TUNING_PARAM(PlayerHooking, player_hooking, 1)

// ddnet tuning
MACRO_TUNING_PARAM(JetpackStrength, jetpack_strength, 400.0f)
MACRO_TUNING_PARAM(ShotgunStrength, shotgun_strength, 10.0f)
MACRO_TUNING_PARAM(ExplosionStrength, explosion_strength, 0.5f)
MACRO_TUNING_PARAM(HammerStrength, hammer_strength, 1.0f)
MACRO_TUNING_PARAM(HookDuration, hook_duration, 1.25f)

MACRO_TUNING_PARAM(HammerFireDelay, hammer_fire_delay, 125)
MACRO_TUNING_PARAM(GunFireDelay, gun_fire_delay, 125)
MACRO_TUNING_PARAM(ShotgunFireDelay, shotgun_fire_delay, 500)
MACRO_TUNING_PARAM(GrenadeFireDelay, grenade_fire_delay, 500)
MACRO_TUNING_PARAM(LaserFireDelay, laser_fire_delay, 800)
MACRO_TUNING_PARAM(NinjaFireDelay, ninja_fire_delay, 800)
MACRO_TUNING_PARAM(HammerHitFireDelay, hammer_hit_fire_delay, 320)

MACRO_TUNING_PARAM(GroundElasticityX, ground_elasticity_x, 0)
MACRO_TUNING_PARAM(GroundElasticityY, ground_elasticity_y, 0)

