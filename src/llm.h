#ifndef LLM_H
#define LLM_H

#include <stdint.h>

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

/* ── Synchronous Query ────────────────────────────────────── */
/* Send a query to the LLM via serial bridge and get the response */
int llm_query(const char *question, char *response, int max_len);

/* Set the system prompt used by the LLM bridge */
void llm_set_system_prompt(const char *prompt);

/* ── Asynchronous / Non-Blocking Query ────────────────────── */
/* Send a query without blocking — returns immediately */
void llm_query_async(const char *question);

/* Poll for async response. Returns:
   0 = still waiting, >0 = response received (length), -1 = error/timeout */
int llm_poll_response(char *response, int max_len);

/* Check if an async query is currently pending */
int llm_async_pending(void);

/* Check bytes available in response stream (for streaming display) */
int llm_stream_available(void);

/* Read partial stream data (for streaming token display) */
int llm_stream_read(char *buf, int max_len);

/* ── Telemetry & Heartbeat ────────────────────────────────── */
/* Send telemetry data to bridge (non-blocking) */
void llm_send_telemetry(uint32_t mem_pct, uint32_t proc_count, uint32_t uptime_s, uint32_t ctx_switches);

/* Send heartbeat ping to check bridge connection */
void llm_send_heartbeat(void);

/* Check if bridge is connected (heartbeat response received recently) */
int llm_bridge_connected(void);

/* ── Latency Tracking ─────────────────────────────────────── */
/* Get last query round-trip time in milliseconds */
uint32_t llm_get_last_latency(void);

/* Get number of queries issued this session */
uint32_t llm_get_query_count(void);

/* ── Host Persistent Storage over Bridge ──────────────────── */
void llm_host_save(const char *name, const char *content);
int  llm_host_load(const char *name, char *buf, int max_len);
void llm_host_audit(const char *event);

#endif
