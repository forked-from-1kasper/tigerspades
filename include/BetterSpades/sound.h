/*
    Copyright (c) 2017-2020 ByteBit

    This file is part of BetterSpades.

    BetterSpades is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BetterSpades is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BetterSpades.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SOUND_H
#define SOUND_H

#ifdef USE_SOUND
    #if __APPLE__
        #include <OpenAL/al.h>
        #include <OpenAL/alc.h>
    #else
        #include <AL/al.h>
        #include <AL/alc.h>
    #endif
#endif

#include <BetterSpades/player.h>

#define SOUND_SCALE 0.6F

enum sound_space {
    SOUND_WORLD,
    SOUND_LOCAL,
};

extern int sound_enabled;

typedef struct {
    ALuint openal_buffer;
    float min, max;
} WAV;

extern WAV sound_footstep1;
extern WAV sound_footstep2;
extern WAV sound_footstep3;
extern WAV sound_footstep4;

extern WAV sound_wade1;
extern WAV sound_wade2;
extern WAV sound_wade3;
extern WAV sound_wade4;

extern WAV sound_jump;
extern WAV sound_jump_water;

extern WAV sound_land;
extern WAV sound_land_water;

extern WAV sound_hurt_fall;

extern WAV sound_explode;
extern WAV sound_explode_water;
extern WAV sound_grenade_bounce;
extern WAV sound_grenade_pin;

extern WAV sound_pickup;
extern WAV sound_horn;

extern WAV sound_rifle_shoot;
extern WAV sound_rifle_reload;
extern WAV sound_smg_shoot;
extern WAV sound_smg_reload;
extern WAV sound_shotgun_shoot;
extern WAV sound_shotgun_reload;
extern WAV sound_shotgun_cock;

extern WAV sound_hitground;
extern WAV sound_hitplayer;
extern WAV sound_build;

extern WAV sound_spade_woosh;
extern WAV sound_spade_whack;

extern WAV sound_death;
extern WAV sound_beep1;
extern WAV sound_beep2;
extern WAV sound_switch;
extern WAV sound_empty;
extern WAV sound_intro;

extern WAV sound_debris;
extern WAV sound_bounce;
extern WAV sound_impact;

void sound_volume(float vol);
void sound_create_sticky(WAV * w, Player * player, int player_id);
void sound_create(enum sound_space option, WAV * w, float x, float y, float z);
void sound_update(void);
void sound_load(WAV * wav, char* name, float min, float max);
void sound_init(void);

#endif
