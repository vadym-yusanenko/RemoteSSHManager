/*
 * functions.h
 *
 *  Created on: Aug 24, 2013
 *      Author: vadimyusanenko
 */

#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_

#include <pthread.h>
#include <libssh2.h> /* LIBSSH2_SESSION */
#include "definitions.h" /* struct Command, struct RemoteHost */

#ifdef _WIN32
void thread_setup(void);

void thread_cleanup(void);

void win32_locking_callback(int mode, int type, const char *file, int line);
#endif

const char* hostname_to_ip(const char* hostname);

void form_response_string(char** response_string, char* response_chunk, unsigned long response_chunk_size);

void ssh_disconnect(LIBSSH2_SESSION* session, int* socket);

unsigned char ssh_connect(const char* ssh_host, const char* ssh_username, const char* ssh_password, const char* ssh_key_path, int* ssh_socket, LIBSSH2_SESSION** ssh_session);

void ssh_execute_commands(LIBSSH2_SESSION* ssh_session, int* ssh_socket, struct CommandDetails* commands, struct Command* command_dictionary, struct Response** responses_dictionary);

struct RemoteHost* create_new_remote_host(struct RemoteHost* parent, struct CommandDetails* commands, const char* host_name, const char* ssh_username, const char* ssh_key_path, const char* ssh_password);

long get_remote_host_count(struct RemoteHost* remote_hosts);

struct CommandDetails* create_new_command(struct CommandDetails* parent, long command_id);

struct Command* create_new_dictionary_command(struct Command* parent, const char* command_string);

void* remote_host_operation(void* host_data);

void* execute_remote_host_operations(void* remote_hosts);

void* check_remote_host_operations_status(long process_handle, long* finished_processes, long* active_processes, long* processes_overall);

const char* get_command_from_dictionary_by_id(struct Command* dictionary_commands, long command_id);

long record_response_in_dictionary(struct Response** responses, char* response, enum ResponseType response_type);

void release_response_memory(struct Response** responses);

#endif /* FUNCTIONS_H_ */
