// SPDX-License-Identifier: LGPL-3.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#ifdef XDB_CLIENT_EXPORTS
#define XDB_CLIENT_API __declspec(dllexport)
#else
#define XDB_CLIENT_API __declspec(dllimport)
#endif
#ifdef XDB_SERVER_EXPORTS
#define XDB_SERVER_API __declspec(dllexport)
#else
#define XDB_SERVER_API __declspec(dllimport)
#endif
#else
#define XDB_CLIENT_API __attribute__((visibility("default")))
#define XDB_SERVER_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct xdb_server xdb_server;
typedef struct xdb_client xdb_client;
typedef struct xdb_response xdb_response;
// Callbacks run synchronously within request(). Data is borrowed until return.
// Do not reenter or destroy this client from its event callback.
typedef void (*xdb_event_callback)(const char *json, void *userdata);
// Thread-local diagnostic, borrowed until the next API call on this thread.
XDB_CLIENT_API const char *xdb_last_error(void);
// Loopback only. Port 0 selects a free port. Empty token generates a private token.
// data_directory must be an absolute path owned by the embedding application.
// Stop joins all workers. Must not be called from an event callback.
XDB_SERVER_API const char *xdb_server_last_error(void);
XDB_SERVER_API xdb_server *xdb_server_start(uint16_t port, const char *token, const char *data_directory);
XDB_SERVER_API uint16_t xdb_server_port(const xdb_server *server);
XDB_SERVER_API const char *xdb_server_token(const xdb_server *server);
XDB_SERVER_API void xdb_server_stop(xdb_server *server);
// Client library never launches a process or loads the device backend.
XDB_CLIENT_API xdb_client *xdb_client_connect(uint16_t port, const char *token, xdb_event_callback event,
                                              void *userdata);
// One outstanding request per client. Response owns JSON and any binary payload.
// Returns 1 when a well-formed response was received; inspect its `ok` field.
XDB_CLIENT_API int xdb_client_request(xdb_client *client, const char *method, const char *params_json,
                                      xdb_response **response);
// May interrupt an in-flight request from another thread. Client becomes unusable.
XDB_CLIENT_API void xdb_client_interrupt(xdb_client *client);
// Call after requests/interrupt callers have returned.
XDB_CLIENT_API void xdb_client_destroy(xdb_client *client);
XDB_CLIENT_API const char *xdb_response_json(const xdb_response *response);
XDB_CLIENT_API const uint8_t *xdb_response_data(const xdb_response *response);
XDB_CLIENT_API size_t xdb_response_size(const xdb_response *response);
XDB_CLIENT_API void xdb_response_destroy(xdb_response *response);
#ifdef __cplusplus
}
#endif
