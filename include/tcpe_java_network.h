#ifndef TCPE_JAVA_NETWORK_H
#define TCPE_JAVA_NETWORK_H
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct TcpeJavaAccountStatus {uint64_t revision;uint32_t phase;bool available;unsigned char username[17],message[256],user_code[33],verification_uri[256];uint64_t expires_at;} TcpeJavaAccountStatus;
bool tcpe_java_account_status(TcpeJavaAccountStatus* result);
bool tcpe_java_account_action(uint32_t action);
typedef struct TcpeJavaSession TcpeJavaSession;
bool tcpe_java_uses_account(const TcpeJavaSession* session);
typedef struct TcpeJavaStatusRequest TcpeJavaStatusRequest;
typedef struct TcpeJavaStatusView {const uint8_t* motd;size_t motd_len;const uint8_t* icon;size_t icon_len;int32_t online,max,latency;} TcpeJavaStatusView;
TcpeJavaStatusRequest* tcpe_java_status_start(const char* host,size_t len,uint16_t port);
int32_t tcpe_java_status_poll(TcpeJavaStatusRequest*,TcpeJavaStatusView*);
void tcpe_java_status_free(TcpeJavaStatusRequest*);

enum TcpeJavaEventKind {
 TCPE_JAVA_LOGIN=1, TCPE_JAVA_SPAWN=2, TCPE_JAVA_POSITION=3,
 TCPE_JAVA_CHUNK=4, TCPE_JAVA_UNLOAD_CHUNK=5, TCPE_JAVA_ENTITIES=6,
 TCPE_JAVA_INVENTORY=7, TCPE_JAVA_CHAT=8, TCPE_JAVA_CHAT_ACTIONS=9,
 TCPE_JAVA_CONFIGURATION=10, TCPE_JAVA_DIALOG=11, TCPE_JAVA_DISCONNECTED=12,
 TCPE_JAVA_ERROR=13, TCPE_JAVA_PLAYER_ID=14, TCPE_JAVA_TIME=15,
 TCPE_JAVA_VISUAL_EFFECT=33, TCPE_JAVA_DELETE_CHAT=34, TCPE_JAVA_SOUND=35, TCPE_JAVA_TARGETING=36,
 TCPE_JAVA_VITALS=16, TCPE_JAVA_HURT=17, TCPE_JAVA_DEATH=18, TCPE_JAVA_LIGHT=20, TCPE_JAVA_AUTHENTICATION=19, TCPE_JAVA_SKIN=21, TCPE_JAVA_LOOK_CORRECTION=22, TCPE_JAVA_REMOVED_ENTITIES=23, TCPE_JAVA_ACTION_BAR=24, TCPE_JAVA_TITLE=25, TCPE_JAVA_BOSS=26, TCPE_JAVA_SCOREBOARD=27, TCPE_JAVA_RESOURCE_PACKS=28, TCPE_JAVA_PACK_FONTS=29, TCPE_JAVA_PACK_FONT_ERROR=30, TCPE_JAVA_COMPASS=31, TCPE_JAVA_RECIPES=32
};
typedef struct TcpeJavaEvent {
 uint32_t kind; int32_t x,z,min_y; uint32_t height;
 const uint8_t* payload; size_t payload_len;
 const uint16_t* states; size_t states_len;
 /* Dense nibble light planes: index=((y-min_y)*16+z)*16+x;
    even indices use low nibble. Section flags: 1 sky, 2 block;
    missing flag preserves prior lighting, explicit empty masks supply zero. */
 const uint8_t* sky_light; size_t sky_light_len;
 const uint8_t* block_light; size_t block_light_len;
 const uint8_t* light_sections; size_t light_sections_len;
} TcpeJavaEvent;
/* Addresses and identity are UTF8 with explicit lengths. Online identity is email. */
TcpeJavaSession* tcpe_java_connect(const char* address,size_t address_len,const char* identity,size_t identity_len,bool online);
/* Single owning game thread. Close a session exactly once. */
void tcpe_java_close(TcpeJavaSession* session);
bool tcpe_java_command(TcpeJavaSession* session,const char* json,size_t len);
/* 1=event, 0=no event, -1=closed/invalid. Buffers expire at next poll or close.
   Payload is UTF8 JSON. Chunk states retain the full advertised height/min_y.
   State index = (local_x * 16 + local_z) * height + (y - min_y). */
int32_t tcpe_java_poll(TcpeJavaSession* session,TcpeJavaEvent* event);
typedef struct TcpeJavaText {const uint8_t* data;size_t len;} TcpeJavaText;
/* Typed current-event getters; dot paths support object fields and array indices.
   Text expires at the next poll/close. Missing or wrong-type fields return false. */
bool tcpe_java_number(const TcpeJavaSession*,const char* path,size_t len,double* result);
bool tcpe_java_boolean(const TcpeJavaSession*,const char* path,size_t len,bool* result);
bool tcpe_java_text(const TcpeJavaSession*,const char* path,size_t len,TcpeJavaText* result);
bool tcpe_java_array_size(const TcpeJavaSession*,const char* path,size_t len,size_t* result);
#ifdef __cplusplus
}
#endif
#endif
