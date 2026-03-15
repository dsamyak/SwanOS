#ifndef LLM_H
#define LLM_H

/* ── LLM Subsystem ────────────────────────────────────────── */

/* Initialize the LLM subsystem — loads API key from filesystem */
void llm_init(void);

/* Set/get the API key (stored in filesystem as .apikey) */
void llm_set_api_key(const char *key);
const char *llm_get_api_key(void);

/* Returns 1 if an API key is configured, 0 otherwise */
int llm_ready(void);

/* Send API key to the serial bridge so it can authenticate */
void llm_send_key(void);

/* Send a query to the LLM via serial bridge and get the response */
int llm_query(const char *question, char *response, int max_len);

#endif
