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

#include <math.h>

#include <BetterSpades/particle.h>
#include <BetterSpades/weapon.h>
#include <BetterSpades/camera.h>
#include <BetterSpades/tracer.h>
#include <BetterSpades/window.h>
#include <BetterSpades/map.h>

float weapon_reload_start, weapon_last_shot;
unsigned char weapon_reload_inprogress = 0;

void weapon_update() {
    float t, delay = weapon_delay(players[local_player.id].weapon);
    int bullets = weapon_can_reload();
    switch (players[local_player.id].weapon) {
        case WEAPON_RIFLE:   t = 2.5F;           break;
        case WEAPON_SMG:     t = 2.5F;           break;
        case WEAPON_SHOTGUN: t = 0.5F * bullets; break;
    }

    if (weapon_reload_inprogress) {
        if (players[local_player.id].weapon == WEAPON_SHOTGUN) {
            if (window_time() - weapon_reload_start >= 0.5F) {

                WAV * wav = NULL;
                if (local_player.ammo < 6 && local_player.ammo_reserved > 0) {
                    local_player.ammo++;
                    local_player.ammo_reserved--;

                    weapon_reload_start = window_time();
                    wav = sound(SOUND_SHOTGUN_RELOAD);
                } else {
                    weapon_reload_inprogress = 0;
                    wav = sound(SOUND_SHOTGUN_COCK);
                }

                sound_create(SOUND_LOCAL, wav, 0.0F, 0.0F, 0.0F);
            }
        } else if (window_time() - weapon_reload_start >= t) {
            local_player.ammo += bullets;
            local_player.ammo_reserved -= bullets;
            weapon_reload_inprogress = 0;
        }

        if (players[local_player.id].held_item == TOOL_GUN) {
            players[local_player.id].item_disabled    = weapon_reload_inprogress ? window_time() : 0;
            players[local_player.id].items_show_start = window_time();
            players[local_player.id].items_show       = 1;
        }
    } else {
        if (screen_current == SCREEN_NONE && window_time() - players[local_player.id].item_disabled >= 0.5F) {
            if (HASBIT(players[local_player.id].input.buttons, BUTTON_PRIMARY) &&
               (players[local_player.id].held_item == TOOL_GUN) &&
               (local_player.ammo > 0) &&
               (window_time() - weapon_last_shot >= delay)) {
                weapon_shoot();
                #if !(HACKS_ENABLED && HACK_NORELOAD)
                local_player.ammo = max(local_player.ammo - 1, 0);
                #endif
                weapon_last_shot = window_time();
            }
        }
    }
}

float weapon_recoil_anim(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return 0.3F;
        case WEAPON_SMG:     return 0.125F;
        case WEAPON_SHOTGUN: return 0.75F;
        default:             return 0.0F;
    }
}

int weapon_block_damage(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return 50;
        case WEAPON_SMG:     return 34;
        case WEAPON_SHOTGUN: return 20;
        default:             return 0;
    }
}

float weapon_delay(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return 0.5F;
        case WEAPON_SMG:     return 0.1F;
        case WEAPON_SHOTGUN: return 1.0F;
        default:             return 0.0F;
    }
}

WAV * weapon_sound(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return sound(SOUND_RIFLE_SHOOT);
        case WEAPON_SMG:     return sound(SOUND_SMG_SHOOT);
        case WEAPON_SHOTGUN: return sound(SOUND_SHOTGUN_SHOOT);
        default:             return NULL;
    }
}

WAV * weapon_sound_reload(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return sound(SOUND_RIFLE_RELOAD);
        case WEAPON_SMG:     return sound(SOUND_SMG_RELOAD);
        case WEAPON_SHOTGUN: return sound(SOUND_SHOTGUN_RELOAD);
        default:             return NULL;
    }
}

void weapon_spread(Player * p, float * d) {
    float spread = 0.0F;
    switch (p->weapon) {
        case WEAPON_RIFLE:   spread = 0.006F; break;
        case WEAPON_SMG:     spread = 0.012F; break;
        case WEAPON_SHOTGUN: spread = 0.024F; break;
    }

#if !(HACKS_ENABLED && HACK_NOSPREAD)
    float scope  = HASBIT(p->input.buttons, BUTTON_SECONDARY) ? 0.5F : 1.0F;
    float crouch = (HASBIT(p->input.keys, INPUT_CROUCH) && p->weapon != WEAPON_SHOTGUN) ? 0.5F : 1.0F;

    d[0] += (ms_rand() - ms_rand()) / 16383.0F * spread * scope * crouch;
    d[1] += (ms_rand() - ms_rand()) / 16383.0F * spread * scope * crouch;
    d[2] += (ms_rand() - ms_rand()) / 16383.0F * spread * scope * crouch;
#endif
}

void weapon_recoil(int gun, double * horiz_recoil, double * vert_recoil) {
    switch (gun) {
        case WEAPON_RIFLE:
            *horiz_recoil = 0.0001;
            *vert_recoil  = 0.05;
            break;
        case WEAPON_SMG:
            *horiz_recoil = 0.00005;
            *vert_recoil  = 0.0125;
            break;
        case WEAPON_SHOTGUN:
            *horiz_recoil = 0.0002;
            *vert_recoil  = 0.1;
            break;
        default:
            *horiz_recoil = 0.0;
            *vert_recoil  = 0.0;
    }
}

int weapon_ammo(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return 10;
        case WEAPON_SMG:     return 30;
        case WEAPON_SHOTGUN: return 6;
        default:             return 0;
    }
}

int weapon_ammo_reserved(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return 50;
        case WEAPON_SMG:     return 120;
        case WEAPON_SHOTGUN: return 48;
        default:             return 0;
    }
}

kv6 * weapon_casing(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return &model[MODEL_SEMI_CASING];
        case WEAPON_SMG:     return &model[MODEL_SMG_CASING];
        case WEAPON_SHOTGUN: return &model[MODEL_SHOTGUN_CASING];
        default:             return NULL;
    }
}

kv6 * weapon_model(int gun) {
    switch (gun) {
        case WEAPON_RIFLE:   return &model[MODEL_SEMI];
        case WEAPON_SMG:     return &model[MODEL_SMG];
        case WEAPON_SHOTGUN: return &model[MODEL_SHOTGUN];
        default:             return NULL;
    }
}

void weapon_set(bool restock) {
    if (!restock) local_player.ammo = weapon_ammo(players[local_player.id].weapon);

    local_player.ammo_reserved = weapon_ammo_reserved(players[local_player.id].weapon);
    weapon_reload_inprogress = 0;
}

void weapon_reload() {
    if (local_player.ammo_reserved == 0 || weapon_reload_inprogress || !weapon_can_reload())
        return;

    weapon_reload_start = window_time();
    weapon_reload_inprogress = 1;

    sound_create(SOUND_LOCAL, weapon_sound_reload(players[local_player.id].weapon),
        players[local_player.id].pos.x,
        players[local_player.id].pos.y,
        players[local_player.id].pos.z
    );

    PacketWeaponReload contained;
    contained.player_id = local_player.id;
    contained.ammo      = local_player.ammo;
    contained.reserved  = local_player.ammo_reserved;

    sendPacketWeaponReload(&contained, 0);
}

void weapon_reload_abort() {
    if (weapon_reload_inprogress && players[local_player.id].weapon == WEAPON_SHOTGUN) {
        weapon_reload_inprogress               = 0;
        players[local_player.id].items_show    = 0;
        players[local_player.id].item_showup   = 0;
        players[local_player.id].item_disabled = 0;
    }
}

int weapon_reloading() {
    return weapon_reload_inprogress;
}

int weapon_can_reload() {
    int mag_size = weapon_ammo(players[local_player.id].weapon);
    return clamp(0, min(local_player.ammo_reserved, mag_size), mag_size - local_player.ammo);
}

#define WEAPON_ROUNDS(w) ((w) == WEAPON_SHOTGUN ? 8 : 1)

void weapon_shoot() {
    // see this for reference:
    // https://pastebin.com/raw/TMjKSTXG
    // http://paste.quacknet.org/view/a3ea2743

    for (int i = 0; i < WEAPON_ROUNDS(players[local_player.id].weapon); i++) {
        float o[3] = {
            players[local_player.id].orientation.x,
            players[local_player.id].orientation.y,
            players[local_player.id].orientation.z
        };

        weapon_spread(&players[local_player.id], o);

        CameraHit hit;
        camera_hit(&hit, local_player.id,
            players[local_player.id].physics.eye.x,
            players[local_player.id].physics.eye.y + player_height(&players[local_player.id]),
            players[local_player.id].physics.eye.z,
            o[X], o[Y], o[Z], 128.0F
        );

        #if !(HACKS_ENABLED && HACK_NORELOAD)
        if (players[local_player.id].input.buttons != network_buttons_last) {
            PacketWeaponInput contained;
            contained.player_id = local_player.id;
            contained.input     = players[local_player.id].input.buttons;
            sendPacketWeaponInput(&contained, 0);

            network_buttons_last = players[local_player.id].input.buttons;
        }
        #endif

        {
            PacketOrientationData contained;
            contained.orient = htonov3f(players[local_player.id].orientation);
            sendPacketOrientationData(&contained, 0);
        }

        if (hit.y == 0 && hit.type == CAMERA_HITTYPE_BLOCK)
            hit.type = CAMERA_HITTYPE_NONE;

        switch (hit.type) {
            case CAMERA_HITTYPE_PLAYER: {
                sound_create_sticky(
                    sound(hit.player_section == HITTYPE_HEAD ? SOUND_SPADE_WHACK : SOUND_HITPLAYER),
                    players + hit.player_id, hit.player_id
                );

                particle_create(
                    Red,
                    players[hit.player_id].physics.eye.x,
                    players[hit.player_id].physics.eye.y + player_section_height(hit.player_section),
                    players[hit.player_id].physics.eye.z,
                    3.5F, 1.0F, 8, 0.1F, 0.4F
                );

                PacketHit contained;
                contained.player_id = hit.player_id;
                #if HACK_HEADSHOT
                contained.hit_type = HITTYPE_HEAD;
                #else
                contained.hit_type = hit.player_section;
                #endif
                sendPacketHit(&contained, 0);

                break;
            }

            case CAMERA_HITTYPE_BLOCK: {
                map_damage(hit.x, hit.y, hit.z, weapon_block_damage(players[local_player.id].weapon));
                if (map_damage_action(hit.x, hit.y, hit.z) && hit.y > 1) {
                    PacketBlockAction contained;
                    contained.action_type = ACTION_DESTROY;
                    contained.player_id   = local_player.id;
                    contained.pos.x       = hit.x;
                    contained.pos.y       = hit.z;
                    contained.pos.z       = 63 - hit.y;
                    sendPacketBlockAction(&contained, 0);
                } else {
                    particle_create(
                        map_get(hit.x, hit.y, hit.z),
                        hit.xb + 0.5F, hit.yb + 0.5F, hit.zb + 0.5F,
                        2.5F, 1.0F, 4, 0.1F, 0.25F
                    );
                }
                break;
            }
        }

        tracer_pvelocity(o, &players[local_player.id]);
        tracer_add(
            players[local_player.id].weapon,
            players[local_player.id].physics.eye.x,
            players[local_player.id].physics.eye.y + player_height(&players[local_player.id]),
            players[local_player.id].physics.eye.z,
            o[0], o[1], o[2]
        );
    }

    double horiz_recoil, vert_recoil;
    weapon_recoil(players[local_player.id].weapon, &horiz_recoil, &vert_recoil);

    long triangle_wave = (long) (window_time() * 1000) & 511;
    horiz_recoil *= ((double) triangle_wave) - 255.5;

    if (((long) (window_time() * 1000) & 1023) < 512)
        horiz_recoil *= -1.0;

    if (ISMOVING(&players[local_player.id]) && !HASBIT(players[local_player.id].input.buttons, BUTTON_SECONDARY)) {
        vert_recoil  *= 2.0;
        horiz_recoil *= 2.0;
    }

    if (players[local_player.id].physics.airborne) {
        vert_recoil  *= 2.0;
        horiz_recoil *= 2.0;
    } else if (HASBIT(players[local_player.id].input.keys, INPUT_CROUCH)) {
        vert_recoil  *= 0.5;
        horiz_recoil *= 0.5;
    }

    // converges to 0 for (+/-) 1.0 (only slowly, has no effect on usual orientation.y range)
    horiz_recoil *= sqrtf(1.0F - fourthf(players[local_player.id].orientation.y));

#if !(HACKS_ENABLED && HACK_NORECOIL)
    camera.rot.x += horiz_recoil;
    camera.rot.y -= vert_recoil;
#endif

    camera_overflow_adjust();

    sound_create(
        SOUND_LOCAL, weapon_sound(players[local_player.id].weapon),
        players[local_player.id].pos.x,
        players[local_player.id].pos.y,
        players[local_player.id].pos.z
    );

    particle_create_casing(&players[local_player.id]);
}
