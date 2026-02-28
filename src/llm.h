#ifndef LLM_H
#define LLM_H

/* Send a query to the LLM via serial bridge and get the response */
int llm_query(const char *question, char *response, int max_len);

#endif
