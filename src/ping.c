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

#define _XOPEN_SOURCE 600

#include <enet/enet.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#include <hashtable.h>

#include <BetterSpades/config.h>
#include <BetterSpades/window.h>
#include <BetterSpades/ping.h>
#include <BetterSpades/common.h>
#include <BetterSpades/list.h>
#include <BetterSpades/hud.h>
#include <BetterSpades/channel.h>
#include <BetterSpades/utils.h>

Channel ping_queue;
ENetSocket sock, lan;
pthread_t ping_thread;
void (*ping_result)(void *, float time_delta, char * aos);

void ping_init() {
    channel_create(&ping_queue, sizeof(PingEntry), 64);

    sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);

    lan = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    enet_socket_set_option(lan, ENET_SOCKOPT_NONBLOCK, 1);
    enet_socket_set_option(lan, ENET_SOCKOPT_BROADCAST, 1);

    pthread_create(&ping_thread, NULL, ping_update, NULL);
}

void ping_deinit() {
    enet_socket_destroy(sock);
    enet_socket_destroy(lan);
}

float lan_ping_start = 0.0F;

static void ping_lan() {
    static const ENetBuffer hellolan = {.data = "HELLOLAN", .dataLength = 8};
    static ENetAddress addr = {.host = 0xFFFFFFFF}; // 255.255.255.255

    lan_ping_start = window_time();

    int begin = max(0, settings.min_lan_port), end = min(65535, settings.max_lan_port);

    for (addr.port = begin; addr.port <= end; addr.port++)
        enet_socket_send(lan, &addr, &hellolan, 1);
}

static bool pings_retry(void * key, void * value, void * user) {
    PingEntry * entry = (PingEntry *) value;

    if (window_time() - entry->time_start > 2.0F) { // timeout
        // try up to 3 times after first failed attempt
        if (entry->trycount >= 3) {
            return true;
        } else {
            enet_socket_send(sock, &entry->addr, &(ENetBuffer) {.data = "HELLO", .dataLength = 5}, 1);
            entry->time_start = window_time();
            entry->trycount++;
            log_warn("Ping timeout on %s, retrying (attempt %i)", entry->aos, entry->trycount);
        }
    }

    return false;
}

#define IP_KEY(addr) (((uint64_t) addr.host << 16) | (addr.port));

Version json_get_game_version(const JSON_Object * obj) {
    const char * game_version = json_object_get_string(obj, "game_version");

    if (game_version != NULL) {
        if (strcmp(game_version, "0.75") == 0)
            return VER075;

        if (strcmp(game_version, "0.76") == 0)
            return VER076;
    }

    return UNKNOWN;
}

void * ping_update(void * data) {
    pthread_detach(pthread_self());

    float ping_start = window_time();

    HashTable pings; ht_setup(&pings, sizeof(uint64_t), sizeof(PingEntry), 64);

    for (;;) {
        size_t drain = channel_size(&ping_queue);
        for (size_t k = 0; (k < drain) || (!pings.size && window_time() - ping_start >= 8.0F); k++) {
            PingEntry entry;
            channel_await(&ping_queue, &entry);

            uint64_t ID = IP_KEY(entry.addr);
            ht_insert(&pings, &ID, &entry);
        }

        char tmp[512];
        ENetAddress from;

        ENetBuffer buf = {.data = tmp, .dataLength = sizeof(tmp)};

        for (;;) {
            int recvLength = enet_socket_receive(sock, &from, &buf, 1);
            uint64_t ID = IP_KEY(from);

            if (recvLength != 0) {
                PingEntry * entry = ht_lookup(&pings, &ID);

                if (entry) {
                    if (recvLength > 0) { // received something!
                        if (!strncmp(buf.data, "HI", recvLength)) {
                            ping_result(NULL, window_time() - entry->time_start, entry->aos);
                            ht_erase(&pings, &ID);
                        } else entry->trycount++;
                    } else ht_erase(&pings, &ID); // connection was closed
                }
            } else break; // would block
        }

        ht_iterate_remove(&pings, NULL, pings_retry);

        if (enet_socket_receive(lan, &from, &buf, 1) > 0) {
            // “ping_result” before can block, so sometimes this value is very inaccurate
            float ping = window_time() - lan_ping_start;

            JSON_Value * js = json_parse_string(buf.data);
            if (js) {
                JSON_Object * root = json_value_get_object(js);

                Server e;

                strcpy(e.country, "LAN");
                snprintf(e.identifier, sizeof(e.identifier) - 1, "aos://%u:%u", from.host, from.port);

                strnzcpy(e.name,     json_object_get_string(root, "name"),      sizeof(e.name));
                strnzcpy(e.gamemode, json_object_get_string(root, "game_mode"), sizeof(e.gamemode));
                strnzcpy(e.map,      json_object_get_string(root, "map"),       sizeof(e.map));

                e.current = json_object_get_number(root, "players_current");
                e.max     = json_object_get_number(root, "players_max");
                e.version = json_get_game_version(root);
                e.ping    = ceil(ping * 1000.0F);

                if (ping_result) ping_result(&e, ping, NULL);
                json_value_free(js);
            }
        }

        usleep(1000);
    }
}

void ping_check(char * addr, int port, char * aos) {
    static const ENetBuffer hello = {.data = "HELLO", .dataLength = 5};

    PingEntry entry = {
        .trycount   = 0,
        .addr.port  = port,
        .time_start = window_time(),
    };

    strncpy(entry.aos, aos, sizeof(entry.aos) - 1);
    entry.aos[sizeof(entry.aos) - 1] = 0;

    enet_address_set_host(&entry.addr, addr);

    channel_put(&ping_queue, &entry);

    enet_socket_send(sock, &entry.addr, &hello, 1);
}

void ping_start(void (*result)(void *, float, char *)) {
    ping_stop();
    ping_result = result;
    ping_lan();
}

void ping_stop() {
    channel_clear(&ping_queue);
}
