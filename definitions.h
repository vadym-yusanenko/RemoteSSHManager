/*
 * definitions.h
 *
 *  Created on: Aug 24, 2013
 *      Author: vadimyusanenko
 */

#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

#ifdef _WIN32
#define _WIN32_WINNT  0x501
#endif

#define SSH_SUCCESS 											0
#define SSH_ERROR_FAILED_TO_INITIATE_LIBSSH2 					1
#define SSH_ERROR_FAILED_TO_CONNECT			 					2
#define SSH_ERROR_FAILURE_ESTABLISHING_SESSION 					3
#define SSH_ERROR_PASSWORD_AUTHENTICATION_FAILED 				4
#define SSH_ERROR_KEY_AUTHENTICATION_FAILED 					5
#define SSH_ERROR_NO_SUPPORTED_AUTHENTICATION_METHODS_FOUND 	6

#ifdef DEBUG
#define DEBUG_OUTPUT fprintf
#define DEBUG_OUTPUT_MUTEX_LOCK pthread_mutex_lock
#define DEBUG_OUTPUT_MUTEX_UNLOCK pthread_mutex_unlock
#else
#define DEBUG_OUTPUT if (0)
#define DEBUG_OUTPUT_MUTEX_LOCK if (0)
#define DEBUG_OUTPUT_MUTEX_UNLOCK if (0)
#endif

#define MUTEX_LOCK pthread_mutex_lock
#define MUTEX_UNLOCK pthread_mutex_unlock

/* NOTE:
 * not also this depends on allowed max threads per process in OS,
 * but higher setting will make locking act as crazy
 */
#define THREADS_ALLOWED 100

struct CommandDetails
{
	long command_id;
	long stdout_response_id;
	long stderr_response_id;
	struct CommandDetails* next_command;
};

enum HostStatus
{
	in_queue = 0,
	processing = 1,
	processing_finished = 2,
	ready = 3
};

struct RemoteHost
{
	char* host_name;
	char* ssh_username;
	char* ssh_password;
	char* ssh_key_path;
	struct CommandDetails* commands;
	enum HostStatus status;
	struct RemoteHost* next_remote_host;
	unsigned char error_code;
};

struct Command
{
	char* command;
	struct Command* next_command;
};

enum ResponseType
{
	response_type_stdout,
	response_type_stderr
};

struct Response
{
	char* response;
	enum ResponseType type;
	struct Response* next_response;
};

struct SSHData
{
	struct RemoteHost* remote_hosts;
	struct Command* commands_dictionary;
	struct Response** responses_dictionary;
};

struct RemoteHostData
{
	struct RemoteHost* remote_host;
	struct Command* commands_dictionary;
	struct Response** responses_dictionary;
};

#endif /* DEFINITIONS_H_ */
