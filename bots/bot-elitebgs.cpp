#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libdiscord.h>
#include <orka-utils.h>
#include <orka-user-agent.hpp>

#define ELITEBGS_API_URL "https://elitebgs.app/api/ebgs/v5"

/* ELITEBGS User Agent for performing connections to the API */
orka::user_agent::dati elitebgs_ua;

struct doc_s {
  char name[512];
  char government[512];
  char updated_at[512];
};

struct faction_presence_s {
  char system_id[512];
  char state[512];
  float influence;
  char happiness[512];
  char updated_at[512];
};

struct state_s {
  char state[512];
  char trend[512];
};


void embed_from_json(char *str, size_t len, void *p_embed)
{
  using namespace discord::channel::embed;

  dati *embed = (dati*)p_embed;

  struct doc_s *doc = (struct doc_s*)malloc(sizeof *doc);
  struct sized_buffer **l_docs = NULL; // get docs token from JSON

  struct faction_presence_s *fpresence = (struct faction_presence_s*)malloc(sizeof *fpresence);
  struct faction_presence_s *history = (struct faction_presence_s*)malloc(sizeof *history);
  struct sized_buffer **l_fpresence = NULL; // get faction_presence token from JSON
  struct sized_buffer **l_history = NULL; // get recovering_states token from JSON

  struct state_s *state = (struct state_s*)malloc(sizeof *state);
  struct sized_buffer **l_active_states = NULL; // get active_states token from JSON
  struct sized_buffer **l_pending_states = NULL; // get pending_states token from JSON
  struct sized_buffer **l_recovering_states = NULL; // get recovering_states token from JSON


  int total, page, pages, pagingCounter;
  bool hasPrevPage, hasNextPage;
  char *prevPage, *nextPage;
  json_scanf(str, len,
     "[docs]%L"
     "[total]%d"
     "[page]%d"
     "[pages]%d"
     "[pagingCounter]%d"
     "[hasPrevPage]%b"
     "[hasNextPage]%b"
     "[prevPage]%?s"
     "[nextPage]%?s",
     &l_docs,
     &total,
     &page,
     &pages,
     &pagingCounter,
     &hasPrevPage,
     &hasNextPage,
     &prevPage,
     &nextPage);

  /* @todo add some checks here */

  char field_value[EMBED_FIELD_VALUE_LEN];

  for (size_t i=0; l_docs[i]; ++i) 
  {
    json_scanf(l_docs[i]->start, l_docs[i]->size,
        "[name]%S"
        "[government]%S"
        "[faction_presence]%L"
        "[updated_at]%S"
        "[history]%L",
        doc->name,
        doc->government,
        &l_fpresence,
        doc->updated_at,
        &l_history);

    if (l_fpresence[0]) 
    {
      for (size_t j=0; l_fpresence[j]; ++j)
      {
        json_scanf(l_fpresence[j]->start, l_fpresence[j]->size,
            "[system_id]%S"
            "[state]%S"
            "[influence]%f"
            "[happiness]%S"
            "[active_states]%L"
            "[pending_states]%L"
            "[recovering_states]%L"
            "[updated_at]%S",
            fpresence->system_id,
            fpresence->state,
            &fpresence->influence,
            fpresence->happiness,
            &l_active_states,
            &l_pending_states,
            &l_recovering_states,
            fpresence->updated_at);

        for (size_t j2=0; l_history[j2]; ++j2)
        {
          json_scanf(l_history[j2]->start, l_history[j2]->size,
              "[system_id]%S"
              "[influence]%f"
              "[updated_at]%S",
              history->system_id,
              &history->influence,
              history->updated_at);

          if (0 == strcmp(history->system_id, fpresence->system_id))
            break;
        }

        float influence_diff = 100*(fpresence->influence - history->influence);
        char *influence_emoji;
        if (influence_diff > 0)
          influence_emoji = "📈";
        else if (influence_diff < 0)
          influence_emoji = "📉";
        else
          influence_emoji = "🔷";

        int ret = snprintf(field_value, sizeof(field_value), 
                    "State: %s\n"
                    "Influence: %.1f%s%.1f\n"
                    "Happiness: %s\n",
                    fpresence->state,
                    fpresence->influence * 100, influence_emoji, influence_diff,
                    fpresence->happiness);

        ret += snprintf(&field_value[ret], sizeof(field_value) - ret, "Active States:");
        if (l_active_states[0]) 
        {
          for (size_t k=0; l_active_states[k]; ++k) 
          {
            json_scanf(l_active_states[k]->start, l_active_states[k]->size,
                "[state]%S", state->state);
            ret += snprintf(&field_value[ret], sizeof(field_value) - ret, " %s,", state->state);
          }
          field_value[ret-1] = '\n'; //replace end comma with newline
        }
        else {
          ret += snprintf(&field_value[ret], sizeof(field_value) - ret, " none\n");
        }

        ret += snprintf(&field_value[ret], sizeof(field_value) - ret, "Pending States:");
        if (l_pending_states[0]) 
        {
          for (size_t k=0; l_pending_states[k]; ++k) 
          {
            json_scanf(l_pending_states[k]->start, l_pending_states[k]->size,
                "[state]%S", state->state);
            ret += snprintf(&field_value[ret], sizeof(field_value) - ret, " %s,", state->state);
          }
          field_value[ret-1] = '\n'; //replace end comma with newline
        }
        else {
          ret += snprintf(&field_value[ret], sizeof(field_value) - ret, " none\n");
        }

        ret += snprintf(&field_value[ret], sizeof(field_value) - ret, "Recovering States:");
        if (l_recovering_states[0]) 
        {
          for (size_t k=0; l_recovering_states[k]; ++k) 
          {
            json_scanf(l_recovering_states[k]->start, l_recovering_states[k]->size,
                "[state]%S [trend]%S", state->state, state->trend);
            ret += snprintf(&field_value[ret], sizeof(field_value) - ret, " %s,", state->state);
            //@todo use trend
          }
          field_value[ret-1] = '\n'; //replace end comma with newline
        }
        else {
          ret += snprintf(&field_value[ret], sizeof(field_value) - ret, " none\n");
        }
      }

      free(l_active_states);
      l_active_states = NULL;

      free(l_pending_states);
      l_pending_states = NULL;

      free(l_recovering_states);
      l_recovering_states = NULL;
    }

    free(l_history);
    free(l_fpresence);
    l_fpresence = NULL;

    add_field(embed, doc->name, field_value, true);
  }

  free(doc);
  free(history);
  free(fpresence);
  free(state);

  if (prevPage)
    free(prevPage);
  if (nextPage)
    free(nextPage);
  free(l_docs);
}

void on_ready(discord::client *client, const discord::user::dati *me)
{
  fprintf(stderr, "\n\nEddbapi-Bot succesfully connected to Discord as %s#%s!\n\n",
      me->username, me->discriminator);

  (void)client;
}

void on_command(
    discord::client *client,
    const discord::user::dati *me,
    const discord::channel::message::dati *msg)
{
  using namespace discord::channel;

  // make sure bot doesn't echoes other bots
  if (msg->author->bot)
    return;

  /* Initialize embed struct that will be loaded to  */
  discord::channel::embed::dati new_embed;
  discord::channel::embed::init_dati(&new_embed);

  struct resp_handle resp_handle =
    {&embed_from_json, (void*)&new_embed};

  char query[512];
  int ret = query_inject(query, sizeof(query),
              "(system):s"
              "(count):1", 
              msg->content);

  ASSERT_S(ret < (int)sizeof(query), "Out of bounds write attempt");

  /* Fetch from ELITEBGS API */
  orka::user_agent::run(
      &elitebgs_ua, 
      &resp_handle,
      NULL,
      HTTP_GET,
      "/factions%s", query);

  strncpy(new_embed.title, msg->content, sizeof(new_embed.title));
  new_embed.timestamp = orka_timestamp_ms();
  new_embed.color = 15844367; //gold
  change_footer(&new_embed, "Made with Orka", NULL, NULL);

  message::create::params params = {0};
  params.embed = &new_embed;

  message::create::run(client, msg->channel_id, &params, NULL);

  /* Cleanup resources */
  discord::channel::embed::cleanup_dati(&new_embed);
}

int main(int argc, char *argv[])
{
  const char *config_file;
  if (argc > 1)
    config_file = argv[1];
  else
    config_file = "bot.config";

  /* Initialized ELITEBGS User Agent */
  orka::user_agent::init(&elitebgs_ua, ELITEBGS_API_URL);

  /* Initialize Discord User Agent */
  discord::global_init();
  discord::client *client = discord::fast_init(config_file);
  assert(NULL != client);

  /* Set discord callbacks */
  setcb_ready(client, &on_ready);
  setcb_message_command(client, "!system ", &on_command);

  /* Start a connection to Discord */
  discord::run(client);

  /* Cleanup resources */
  orka::user_agent::cleanup(&elitebgs_ua);
  discord::cleanup(client);
  discord::global_cleanup();

  return EXIT_SUCCESS;
}