#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // for isspace()
#include <pthread.h>

#include "libdiscord.h"
#include "discord-common.h"

#include "orka-utils.h"


#define BASE_GATEWAY_URL "wss://gateway.discord.gg/?v=6&encoding=json"


static char*
opcode_print(int opcode)
{
  switch (opcode) {
      CASE_RETURN_STR(DISCORD_GATEWAY_DISPATCH);
      CASE_RETURN_STR(DISCORD_GATEWAY_HEARTBEAT);
      CASE_RETURN_STR(DISCORD_GATEWAY_IDENTIFY);
      CASE_RETURN_STR(DISCORD_GATEWAY_PRESENCE_UPDATE);
      CASE_RETURN_STR(DISCORD_GATEWAY_VOICE_STATE_UPDATE);
      CASE_RETURN_STR(DISCORD_GATEWAY_RESUME);
      CASE_RETURN_STR(DISCORD_GATEWAY_RECONNECT);
      CASE_RETURN_STR(DISCORD_GATEWAY_REQUEST_GUILD_MEMBERS);
      CASE_RETURN_STR(DISCORD_GATEWAY_INVALID_SESSION);
      CASE_RETURN_STR(DISCORD_GATEWAY_HELLO);
      CASE_RETURN_STR(DISCORD_GATEWAY_HEARTBEAT_ACK);
  default:
      PRINT("Invalid Gateway opcode (code: %d)", opcode);
      return "Invalid Gateway opcode";
  }
}

static char*
close_opcode_print(enum discord_gateway_close_opcodes gateway_opcode)
{
  switch (gateway_opcode) { // check for discord specific opcodes
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_UNKNOWN_ERROR);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_UNKNOWN_OPCODE);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_DECODE_ERROR);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_NOT_AUTHENTICATED);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_AUTHENTICATION_FAILED);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_ALREADY_AUTHENTICATED);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_INVALID_SEQUENCE);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_RATE_LIMITED);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_SESSION_TIMED_OUT);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_INVALID_SHARD);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_SHARDING_REQUIRED);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_INVALID_API_VERSION);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_INVALID_INTENTS);
      CASE_RETURN_STR(DISCORD_GATEWAY_CLOSE_REASON_DISALLOWED_INTENTS);
  default: // check for normal ws_close opcodes
      switch ((enum ws_close_reason)gateway_opcode) {
          CASE_RETURN_STR(WS_CLOSE_REASON_NORMAL);
          CASE_RETURN_STR(WS_CLOSE_REASON_GOING_AWAY);
          CASE_RETURN_STR(WS_CLOSE_REASON_PROTOCOL_ERROR);
          CASE_RETURN_STR(WS_CLOSE_REASON_UNEXPECTED_DATA);
          CASE_RETURN_STR(WS_CLOSE_REASON_NO_REASON);
          CASE_RETURN_STR(WS_CLOSE_REASON_ABRUPTLY);
          CASE_RETURN_STR(WS_CLOSE_REASON_INCONSISTENT_DATA);
          CASE_RETURN_STR(WS_CLOSE_REASON_POLICY_VIOLATION);
          CASE_RETURN_STR(WS_CLOSE_REASON_TOO_BIG);
          CASE_RETURN_STR(WS_CLOSE_REASON_MISSING_EXTENSION);
          CASE_RETURN_STR(WS_CLOSE_REASON_SERVER_ERROR);
          CASE_RETURN_STR(WS_CLOSE_REASON_IANA_REGISTRY_START);
          CASE_RETURN_STR(WS_CLOSE_REASON_IANA_REGISTRY_END);
          CASE_RETURN_STR(WS_CLOSE_REASON_PRIVATE_START);
          CASE_RETURN_STR(WS_CLOSE_REASON_PRIVATE_END);
      default:
          PRINT("Unknown WebSockets close opcode (code: %d)", gateway_opcode);
          return "Unknown WebSockets close opcode";
      }
  }
}

static void
send_payload(struct discord_gateway *gw, char payload[]) {
  ws_send_text(gw->ws, payload);
}

static void
send_resume(struct discord_gateway *gw)
{
  char payload[MAX_PAYLOAD_LEN];
  int ret = json_inject(payload, sizeof(payload), 
              "(op):6" // RESUME OPCODE
              "(d):{"
                "(token):s"
                "(session_id):s"
                "(seq):d"
              "}",
              gw->identify->token,
              gw->session_id, 
              &gw->payload.seq_number);

  ASSERT_S(ret < (int)sizeof(payload), "Out of bounds write attempt");

  D_NOTOP_PRINT("RESUME PAYLOAD:\n\t%s", payload);
  send_payload(gw, payload);
}

static void
send_identify(struct discord_gateway *gw)
{
  /* Ratelimit check */
  pthread_mutex_lock(&gw->lock);
  if ((ws_timestamp(gw->ws) - gw->session.identify_tstamp) < 5) {
    ++gw->session.concurrent;
    VASSERT_S(gw->session.concurrent < gw->session.max_concurrency,
        "Reach identify request threshold (%d every 5 seconds)", gw->session.max_concurrency);
  }
  else {
    gw->session.concurrent = 0;
  }
  pthread_mutex_unlock(&gw->lock);

  char payload[MAX_PAYLOAD_LEN];
  int ret = json_inject(payload, sizeof(payload), 
              "(op):2" // IDENTIFY OPCODE
              "(d):F",
              &discord_gateway_identify_to_json_v, gw->identify);
  ASSERT_S(ret < (int)sizeof(payload), "Out of bounds write attempt");

  // contain token (sensitive data), enable _ORKA_DEBUG_STRICT to print it
  DS_PRINT("IDENTIFY PAYLOAD:\n\t%s", payload);
  send_payload(gw, payload);

  //get timestamp for this identify
  pthread_mutex_lock(&gw->lock);
  gw->session.identify_tstamp = ws_timestamp(gw->ws);
  pthread_mutex_unlock(&gw->lock);
}

static void
on_hello_cb(void *p_gw, void *curr_iter_data)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;
  struct payload_s *payload = (struct payload_s*)curr_iter_data;

  pthread_mutex_lock(&gw->lock);
  gw->hbeat.interval_ms = 0;
  gw->hbeat.tstamp = orka_timestamp_ms();

  json_scanf(payload->event_data, sizeof(payload->event_data),
             "[heartbeat_interval]%ld", &gw->hbeat.interval_ms);
  ASSERT_S(gw->hbeat.interval_ms > 0, "Invalid heartbeat_ms");
  pthread_mutex_unlock(&gw->lock);

  if (WS_RESUME == ws_get_status(gw->ws))
    send_resume(gw);
  else // WS_FRESH || WS_DISCONNECTED
    send_identify(gw);
}

static enum discord_gateway_events
get_dispatch_event(char event_name[])
{
  if (STREQ("READY", event_name)) return DISCORD_GATEWAY_EVENTS_READY;
  if (STREQ("RESUMED", event_name)) return DISCORD_GATEWAY_EVENTS_RESUMED;
  if (STREQ("MESSAGE_REACTION_ADD", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_ADD;
  if (STREQ("MESSAGE_REACTION_REMOVE", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_REMOVE;
  if (STREQ("MESSAGE_REACTION_REMOVE_ALL", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_REMOVE_ALL;
  if (STREQ("MESSAGE_REACTION_REMOVE_EMOJI", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_REMOVE_EMOJI;
  if (STREQ("MESSAGE_CREATE", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_CREATE;
  if (STREQ("MESSAGE_UPDATE", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_UPDATE;
  if (STREQ("MESSAGE_DELETE", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_DELETE;
  if (STREQ("MESSAGE_DELETE_BULK", event_name)) return DISCORD_GATEWAY_EVENTS_MESSAGE_DELETE_BULK;
  if (STREQ("GUILD_MEMBER_ADD", event_name)) return DISCORD_GATEWAY_EVENTS_GUILD_MEMBER_ADD;
  if (STREQ("GUILD_MEMBER_UPDATE", event_name)) return DISCORD_GATEWAY_EVENTS_GUILD_MEMBER_UPDATE;
  if (STREQ("GUILD_MEMBER_REMOVE", event_name)) return DISCORD_GATEWAY_EVENTS_GUILD_MEMBER_REMOVE;
  return DISCORD_GATEWAY_EVENTS_NONE;
}

static void
on_guild_member_add(struct discord_gateway *gw, struct payload_s *payload)
{
  struct discord_guild_member *member = discord_guild_member_alloc();
  discord_guild_member_from_json(payload->event_data,
      sizeof(payload->event_data), member);

  u64_snowflake_t guild_id = 0;
  json_extract(payload->event_data, sizeof(payload->event_data),
    "(guild_id):s_as_u64", &guild_id);

  if (gw->cbs.on_guild_member.add)
    (*gw->cbs.on_guild_member.add)(
        gw->p_client, 
        gw->me, 
        guild_id,
        member);

  discord_guild_member_free(member);
}

static void
on_guild_member_remove(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_guild_member.remove) return;

  u64_snowflake_t guild_id = 0;
  struct discord_user *user = discord_user_alloc();
  json_extract(payload->event_data, sizeof(payload->event_data),
    "(guild_id):s_as_u64"
    "(user):F", 
    &guild_id,
    &discord_user_from_json, user);

  (*gw->cbs.on_guild_member.remove)(
        gw->p_client, 
        gw->me, 
        guild_id, 
        user);

  discord_user_free(user);
}

static void
on_guild_member_update(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_guild_member.update) return;

  struct discord_guild_member *member = discord_guild_member_alloc();
  discord_guild_member_from_json(payload->event_data,
      sizeof(payload->event_data), member);

  u64_snowflake_t guild_id = 0;
  json_extract(payload->event_data, sizeof(payload->event_data),
    "(guild_id):s_as_u64", &guild_id);

  (*gw->cbs.on_guild_member.update)(
      gw->p_client, 
      gw->me, 
      guild_id, 
      member);

  discord_guild_member_free(member);
}

static void
on_message_create(struct discord_gateway *gw, struct payload_s *payload)
{
  struct discord_message *msg = discord_message_alloc();
  discord_message_from_json(payload->event_data,
      sizeof(payload->event_data), msg);

  struct sized_buffer sb_msg = {
    .start = payload->event_data,
    .size = strlen(payload->event_data)
  };

  if (gw->on_cmd) {
    // prefix offset if available
    size_t offset = IS_EMPTY_STRING(gw->prefix) 
                            ? 0 
                            : strlen(gw->prefix);

    message_cb *cmd_cb = NULL;
    char *cmd_str = NULL;
    for (size_t i=0; i < gw->num_cmd; ++i) 
    {
      if (gw->prefix && !STRNEQ(gw->prefix, msg->content, offset))
          continue; //prefix doesn't match msg->content

      // check if command from channel matches set command
      if (STRNEQ(gw->on_cmd[i].str, 
            msg->content + offset, 
            strlen(gw->on_cmd[i].str)))
      {
        cmd_cb = gw->on_cmd[i].cb;
        cmd_str = gw->on_cmd[i].str;
        break;
      }
    }

    if (cmd_cb && cmd_str) {
      char *tmp = msg->content; // hold original ptr

      msg->content = msg->content + offset + strlen(cmd_str);
      while (isspace(*msg->content)) { // offset blank chars
        ++msg->content;
      }

      (*cmd_cb)(gw->p_client, gw->me, msg);

      msg->content = tmp; // retrieve original ptr
    }
  }
  else if (gw->cbs.on_message.sb_create) /* @todo temporary */
    (*gw->cbs.on_message.sb_create)(
      gw->p_client, 
      gw->me, gw->sb_me,
      msg, sb_msg);
  else if (gw->cbs.on_message.create)
    (*gw->cbs.on_message.create)(gw->p_client, gw->me, msg);

  discord_message_free(msg);
}

static void
on_message_update(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_message.update) return;

  struct discord_message *msg = discord_message_alloc();
  discord_message_from_json(payload->event_data,
      sizeof(payload->event_data), msg);

  (*gw->cbs.on_message.update)(gw->p_client, gw->me, msg);

  discord_message_free(msg);
}

static void
on_message_delete(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_message.del) return;

  u64_snowflake_t message_id=0, channel_id=0, guild_id=0;
  json_extract(payload->event_data, sizeof(payload->event_data),
    "(id):s_as_u64"
    "(channel_id):s_as_u64"
    "(guild_id):s_as_u64",
    &message_id, &channel_id, &guild_id);

  (*gw->cbs.on_message.del)(gw->p_client, gw->me, 
      message_id, 
      channel_id, 
      guild_id);
}

static void
on_message_delete_bulk(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_message.delete_bulk) return;

  NTL_T(struct sized_buffer) buf = NULL;
  u64_snowflake_t channel_id = 0, guild_id = 0;
  json_scanf(payload->event_data, sizeof(payload->event_data),
      "[ids]%A"
      "[channel_id]%F"
      "[guild_id]%F",
      &buf,
      &orka_strtoull, &channel_id,
      &orka_strtoull, &guild_id);

  size_t nids = ntl_length((NTL_T(void)) buf);
  u64_snowflake_t *ids = (u64_snowflake_t*)malloc(nids * sizeof(u64_snowflake_t));
  for(size_t i = 0; i < nids; i++) {
    orka_strtoull(buf[i]->start, buf[i]->size, ids + i);
  }

  free(buf);

  (*gw->cbs.on_message.delete_bulk)(gw->p_client, gw->me, nids, ids, channel_id, guild_id);
  free(ids);
}

static void
on_message_reaction_add(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_reaction.add) return;

  u64_snowflake_t user_id=0, message_id=0, channel_id=0, guild_id=0;
  struct discord_guild_member *member = discord_guild_member_alloc();
  struct discord_emoji *emoji = discord_emoji_alloc();

  json_extract(payload->event_data, sizeof(payload->event_data),
      "(user_id):s_as_u64"
      "(message_id):s_as_u64"
      "(member):F"
      "(emoji):F"
      "(channel_id):s_as_u64"
      "(guild_id):s_as_u64",
      &user_id,
      &message_id,
      &discord_guild_member_from_json, member,
      &discord_emoji_from_json, emoji,
      &channel_id,
      &guild_id);

  (*gw->cbs.on_reaction.add)(gw->p_client, gw->me, 
      user_id,
      channel_id, 
      message_id, 
      guild_id, 
      member, 
      emoji);

  discord_guild_member_free(member);
  discord_emoji_free(emoji);
}

static void
on_message_reaction_remove(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_reaction.remove) return;

  u64_snowflake_t user_id=0, message_id=0, channel_id=0, guild_id=0;
  struct discord_emoji *emoji = discord_emoji_alloc();

  json_extract(payload->event_data, sizeof(payload->event_data),
      "(user_id):s_as_u64"
      "(message_id):s_as_u64"
      "(emoji):F"
      "(channel_id):s_as_u64"
      "(guild_id):s_as_u64",
      &user_id,
      &message_id,
      &discord_emoji_from_json, emoji,
      &channel_id,
      &guild_id);

  (*gw->cbs.on_reaction.remove)(gw->p_client, gw->me, 
      user_id,
      channel_id, 
      message_id, 
      guild_id, 
      emoji);

  discord_emoji_free(emoji);
}

static void
on_message_reaction_remove_all(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_reaction.remove_all) return;

  u64_snowflake_t channel_id=0, message_id=0, guild_id=0;
  json_extract(payload->event_data, sizeof(payload->event_data),
      "(channel_id):s_as_u64"
      "(message_id):s_as_u64"
      "(channel_id):s_as_u64",
      &channel_id,
      &message_id,
      &guild_id);

  (*gw->cbs.on_reaction.remove_all)(gw->p_client, gw->me, 
      channel_id, 
      message_id, 
      guild_id);
}

static void
on_message_reaction_remove_emoji(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_reaction.remove_emoji) return;

  u64_snowflake_t channel_id=0, guild_id=0, message_id=0;
  struct discord_emoji *emoji = discord_emoji_alloc();
  json_extract(payload->event_data, sizeof(payload->event_data),
      "(channel_id):s_as_u64"
      "(guild_id):s_as_u64"
      "(message_id):s_as_u64"
      "(emoji):F",
      &channel_id,
      &guild_id,
      &message_id,
      &discord_emoji_from_json, emoji);

    (*gw->cbs.on_reaction.remove_emoji)(gw->p_client, gw->me, 
        channel_id, 
        guild_id, 
        message_id,
        emoji);
}

static void
on_ready(struct discord_gateway *gw, struct payload_s *payload)
{
  if (!gw->cbs.on_ready) return;

  ws_set_status(gw->ws, WS_CONNECTED);
  D_PUTS("Succesfully started a Discord session!");

  json_extract(payload->event_data, sizeof(payload->event_data),
             "(session_id):s", gw->session_id);
  ASSERT_S(gw->session_id, "Missing session_id from READY event");

  (*gw->cbs.on_ready)(gw->p_client, gw->me);
}

static void
on_resumed(struct discord_gateway *gw, struct payload_s *payload)
{
  ws_set_status(gw->ws, WS_CONNECTED);
  PUTS("Succesfully resumed a Discord session!");
}

static void
on_dispatch_cb(void *p_gw, void *curr_iter_data)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;
  struct payload_s *payload = (struct payload_s*)curr_iter_data;

  /* Ratelimit check */
  pthread_mutex_lock(&gw->lock);
  if ((ws_timestamp(gw->ws) - gw->session.event_tstamp) < 60) {
    ++gw->session.event_count;
    ASSERT_S(gw->session.event_count < 120,
        "Reach event dispatch threshold (120 every 60 seconds)");
  }
  else {
    gw->session.event_tstamp = ws_timestamp(gw->ws);
    gw->session.event_count = 0;
  }
  pthread_mutex_unlock(&gw->lock);

  switch(get_dispatch_event(payload->event_name)) {
  case DISCORD_GATEWAY_EVENTS_GUILD_CREATE:
  case DISCORD_GATEWAY_EVENTS_GUILD_UPDATE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_GUILD_DELETE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_GUILD_BAN_ADD:
  case DISCORD_GATEWAY_EVENTS_GUILD_BAN_REMOVE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_GUILD_EMOJIS_UPDATE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_GUILD_INTEGRATIONS_UPDATE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_GUILD_MEMBER_ADD:
      on_guild_member_add(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_GUILD_MEMBER_REMOVE:
      on_guild_member_remove(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_GUILD_MEMBER_UPDATE: 
      on_guild_member_update(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_GUILD_ROLE_CREATE:
  case DISCORD_GATEWAY_EVENTS_GUILD_ROLE_UPDATE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_GUILD_ROLE_DELETE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_INVITE_CREATE:
      //@todo implement
      return;
  case DISCORD_GATEWAY_EVENTS_INVITE_DELETE:
      //@todo implement
      return; 
  case DISCORD_GATEWAY_EVENTS_MESSAGE_CREATE:
      on_message_create(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_MESSAGE_UPDATE:
      on_message_update(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_MESSAGE_DELETE:
      on_message_delete(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_MESSAGE_DELETE_BULK:
      on_message_delete_bulk(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_ADD:
      on_message_reaction_add(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_REMOVE:
      on_message_reaction_remove(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_REMOVE_ALL:
      on_message_reaction_remove_all(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_MESSAGE_REACTION_REMOVE_EMOJI:
      on_message_reaction_remove_emoji(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_READY:
      on_ready(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_RESUMED:
      on_resumed(gw, payload);
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_TYPING_START:
      // @todo implement
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_PRESENCE_UPDATE:
      // @todo implement
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_VOICE_STATE_UPDATE:
      // @todo implement
      return; /* EARLY RETURN */
  case DISCORD_GATEWAY_EVENTS_WEBHOOKS_UPDATE:
      // @todo implement
      return; /* EARLY RETURN */
  default:
      break;
  }

  PRINT("Expected not yet implemented GATEWAY DISPATCH event: %s",
      payload->event_name);
}

static void
on_invalid_session_cb(void *p_gw, void *curr_iter_data)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;
  struct payload_s *payload = (struct payload_s*)curr_iter_data;

  bool is_resumable = strcmp(payload->event_data, "false");
  const char *reason;
  if (is_resumable) {
    ws_set_status(gw->ws, WS_RESUME);
    reason = "Attempting to session resume";
  }
  else {
    ws_set_status(gw->ws, WS_FRESH);
    reason = "Attempting to start a fresh new session";
  }
  PUTS(reason);
  ws_close(gw->ws, WS_CLOSE_REASON_NORMAL, reason, sizeof(reason));
}

static void
on_reconnect_cb(void *p_gw, void *curr_iter_data)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;

  ws_set_status(gw->ws, WS_RESUME);

  const char reason[] = "Attempting to session resume";
  PUTS(reason);
  ws_close(gw->ws, WS_CLOSE_REASON_NORMAL, reason, sizeof(reason));
}

static void
on_heartbeat_ack_cb(void *p_gw, void *curr_iter_data)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;

  // get request / response interval in milliseconds
  pthread_mutex_lock(&gw->lock);
  gw->ping_ms = orka_timestamp_ms() - gw->hbeat.tstamp;
  D_PRINT("PING: %d ms", gw->ping_ms);
  pthread_mutex_unlock(&gw->lock);
}

static void
on_connect_cb(void *p_gw, const char *ws_protocols) {
  D_PRINT("Connected, WS-Protocols: '%s'", ws_protocols);
}

static void
on_close_cb(void *p_gw, enum ws_close_reason wscode, const char *reason, size_t len)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;
  enum discord_gateway_close_opcodes opcode = (enum discord_gateway_close_opcodes)wscode;
 
  switch (opcode) {
  case DISCORD_GATEWAY_CLOSE_REASON_UNKNOWN_OPCODE:
  case DISCORD_GATEWAY_CLOSE_REASON_DECODE_ERROR:
  case DISCORD_GATEWAY_CLOSE_REASON_NOT_AUTHENTICATED:
  case DISCORD_GATEWAY_CLOSE_REASON_AUTHENTICATION_FAILED:
  case DISCORD_GATEWAY_CLOSE_REASON_ALREADY_AUTHENTICATED:
  case DISCORD_GATEWAY_CLOSE_REASON_RATE_LIMITED:
  case DISCORD_GATEWAY_CLOSE_REASON_SHARDING_REQUIRED:
  case DISCORD_GATEWAY_CLOSE_REASON_INVALID_API_VERSION:
  case DISCORD_GATEWAY_CLOSE_REASON_INVALID_INTENTS:
  case DISCORD_GATEWAY_CLOSE_REASON_INVALID_SHARD:
  case DISCORD_GATEWAY_CLOSE_REASON_DISALLOWED_INTENTS:
      ws_set_status(gw->ws, WS_DISCONNECTED);
      break;
  case DISCORD_GATEWAY_CLOSE_REASON_UNKNOWN_ERROR:
  case DISCORD_GATEWAY_CLOSE_REASON_INVALID_SEQUENCE:
      ws_set_status(gw->ws, WS_RESUME);
      break;
  case DISCORD_GATEWAY_CLOSE_REASON_SESSION_TIMED_OUT:
  default: //websocket/clouflare opcodes
      ws_set_status(gw->ws, WS_FRESH);
      break;
  }

  PRINT("%s (code: %4d) : %zd bytes\n\t"
          "REASON: '%s'", 
          close_opcode_print(opcode), opcode, len,
          reason);
}

static void
on_text_cb(void *p_gw, const char *text, size_t len) {
  D_NOTOP_PUTS("FALLBACK TO ON_TEXT");
}

static int
on_startup_cb(void *p_gw)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;

  //get session info before starting it
  discord_get_gateway_bot(gw->p_client, &gw->session);

  if (!gw->session.remaining) {
    PRINT("Reach session starts threshold (%d)\n\t"
          "Please wait %d seconds and try again", 
          gw->session.total, gw->session.reset_after/1000);
    return 0;
  }
  return 1;
}

/* send heartbeat pulse to websockets server in order
 *  to maintain connection alive */
static void
send_heartbeat(struct discord_gateway *gw)
{
  char payload[64];
  int ret = json_inject(payload, sizeof(payload), 
              "(op):1, (d):d", &gw->payload.seq_number);
  ASSERT_S(ret < (int)sizeof(payload), "Out of bounds write attempt");

  D_PRINT("HEARTBEAT_PAYLOAD:\n\t\t%s", payload);
  send_payload(gw, payload);
}

static void
on_iter_end_cb(void *p_gw)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;

  /*check if timespan since first pulse is greater than
   * minimum heartbeat interval required*/
  pthread_mutex_lock(&gw->lock);
  if (gw->hbeat.interval_ms < (ws_timestamp(gw->ws) - gw->hbeat.tstamp)) {
    send_heartbeat(gw);

    gw->hbeat.tstamp = ws_timestamp(gw->ws); //update heartbeat timestamp
  }
  pthread_mutex_unlock(&gw->lock);

  if (gw->cbs.on_idle) {
    (*gw->cbs.on_idle)(gw->p_client, gw->me);
  }
}

static int
on_text_event_cb(void *p_gw, const char *text, size_t len)
{
  struct discord_gateway *gw = (struct discord_gateway*)p_gw;

  D_PRINT("ON_DISPATCH:\t%s\n", text);

  struct payload_s *payloadcpy = calloc(1, sizeof(struct payload_s));

  int tmp_seq_number; //check value first, then assign
  json_scanf((char*)text, len,
              "[t]%s [s]%d [op]%d [d]%S",
               gw->payload.event_name,
               &tmp_seq_number,
               &gw->payload.opcode,
               gw->payload.event_data);

  if (tmp_seq_number) {
    gw->payload.seq_number = tmp_seq_number;
  }

  D_NOTOP_PRINT("OP:\t\t%s\n\t"
                "EVENT NAME:\t%s\n\t"
                "SEQ NUMBER:\t%d\n\t"
                "EVENT DATA:\t%s\n", 
                opcode_print(gw->payload.opcode), 
                *gw->payload.event_name //if event name exists
                   ? gw->payload.event_name //prints event name
                   : "NULL", //otherwise prints NULL
                gw->payload.seq_number,
                gw->payload.event_data);

  memcpy(payloadcpy, &gw->payload, sizeof(struct payload_s));
  ws_set_curr_iter_data(gw->ws, payloadcpy, &free);

  return gw->payload.opcode;
}

void
discord_gateway_init(struct discord_gateway *gw, const char token[], const char config_file[])
{
  struct ws_callbacks cbs = {
    .data = gw,
    .on_startup = &on_startup_cb,
    .on_iter_end = &on_iter_end_cb,
    .on_text_event = &on_text_event_cb,
    .on_connect = &on_connect_cb,
    .on_text = &on_text_cb,
    .on_close = &on_close_cb
  };

  gw->ws = ws_config_init(BASE_GATEWAY_URL, &cbs, "DISCORD GATEWAY", config_file);
  if (config_file) { 
    token = ws_config_get_field(gw->ws, "discord.token");
  }
  if (!token) ERR("Missing bot token");

  ws_set_refresh_rate(gw->ws, 1);
  ws_set_max_reconnect(gw->ws, 15);
  ws_set_event(gw->ws, DISCORD_GATEWAY_HELLO, &on_hello_cb);
  ws_set_event(gw->ws, DISCORD_GATEWAY_DISPATCH, &on_dispatch_cb);
  ws_set_event(gw->ws, DISCORD_GATEWAY_INVALID_SESSION, &on_invalid_session_cb);
  ws_set_event(gw->ws, DISCORD_GATEWAY_RECONNECT, &on_reconnect_cb);
  ws_set_event(gw->ws, DISCORD_GATEWAY_HEARTBEAT_ACK, &on_heartbeat_ack_cb);

  gw->identify = discord_gateway_identify_alloc();
  gw->identify->token = strdup(token);

  gw->identify->properties->$os = strdup("POSIX");
  gw->identify->properties->$browser = strdup("orca");
  gw->identify->properties->$device = strdup("orca");
  gw->identify->presence->since = orka_timestamp_ms();
  gw->me = discord_user_alloc();
  discord_set_presence(gw->p_client, NULL, "online", false);
  discord_get_current_user(gw->p_client, gw->me);
  sb_discord_get_current_user(gw->p_client, &gw->sb_me);

  if (pthread_mutex_init(&gw->lock, NULL))
    ERR("Couldn't initialize pthread mutex");
}

void
discord_gateway_cleanup(struct discord_gateway *gw)
{
  discord_user_free(gw->me);
  discord_gateway_identify_free(gw->identify);
  ws_cleanup(gw->ws);
  pthread_mutex_destroy(&gw->lock);
}

/* connects to the discord websockets server */
void
discord_run(struct discord *client) {
  ws_run(client->gw.ws);
}

void
discord_gateway_shutdown(struct discord_gateway *gw) 
{
  ws_set_status(gw->ws, WS_DISCONNECTED);
  char reason[] = "Shutdown gracefully";
  ws_close(gw->ws, WS_CLOSE_REASON_NORMAL, reason, sizeof(reason));
}