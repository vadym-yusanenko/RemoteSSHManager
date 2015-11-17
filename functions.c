#include "functions.h"

#ifdef _WIN32
#include <Winsock2.h> /* inet_addr(), struct in_addr, struct sockaddr_in, select() */
#include <Ws2tcpip.h> /* getaddrinfo(), struct addrinfo, socklen_t */
#include <WinBase.h> /* Sleep() */

#include <openssl/crypto.h> /* OPENSSL_ and CRYPTO_ symbols */
#else
#include <arpa/inet.h> /* inet_addr() */
#include <netinet/in.h> /* struct in_addr, struct sockaddr_in */
#include <netdb.h> /* getaddrinfo(), struct addrinfo, freeaddrinfo() */
#include <sys/select.h> /* select() */
#include <unistd.h>
#endif

#include <stddef.h> /* NULL */
#include <stdlib.h> /* realloc(), malloc(), free() */
#include <stdio.h> /* fprintf(), stderr */

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef _WIN32
static HANDLE *lock_cs;

void thread_setup(void)
{
	int i;

	lock_cs=OPENSSL_malloc(CRYPTO_num_locks() * sizeof(HANDLE));
	for (i=0; i<CRYPTO_num_locks(); i++)
	{
		lock_cs[i]=CreateMutex(NULL, FALSE, NULL);
	}

	CRYPTO_set_locking_callback((void (*)(int, int, const char *, int)) win32_locking_callback);
}

void thread_cleanup(void)
{
	int i;

	CRYPTO_set_locking_callback(NULL);
	for (i=0; i<CRYPTO_num_locks(); i++)
		CloseHandle(lock_cs[i]);
	OPENSSL_free(lock_cs);
}

void win32_locking_callback(int mode, int type, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
	{
		WaitForSingleObject(lock_cs[type], INFINITE);
	}
	else
	{
		ReleaseMutex(lock_cs[type]);
	}
}
#endif

const char* hostname_to_ip(const char* hostname)
{
	struct in_addr addr;
	memset(&addr, 0, sizeof(addr));

	struct addrinfo *result = NULL;

	getaddrinfo(hostname, NULL, NULL, &result);

	if (result == NULL || result->ai_addr == NULL)
		return ("unknown");

	addr.s_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr;

	freeaddrinfo(result);

	return (inet_ntoa(addr));
}

void form_response_string(char** response_string, char* response_chunk, unsigned long response_chunk_size)
{
	unsigned long response_size = strlen(*response_string);

	*response_string = realloc(*response_string, response_size = (response_size + response_chunk_size + (1 * sizeof(char))));

	memcpy(*response_string + strlen(*response_string), response_chunk, response_chunk_size);
	memset(*response_string + response_size - (1 * sizeof(char)), 0, (1 * sizeof(char)));
}

struct RemoteHost* create_new_remote_host(struct RemoteHost* parent, struct CommandDetails* commands, const char* host_name, const char* ssh_username, const char* ssh_key_path, const char* ssh_password)
{
	struct RemoteHost* remote_host = malloc(sizeof(struct RemoteHost));
	memset(remote_host, 0, sizeof(struct RemoteHost));
	remote_host->host_name = malloc((strlen(host_name) + 1) * sizeof(char));
	strcpy(remote_host->host_name, host_name);
	remote_host->commands = commands;
	remote_host->ssh_key_path = malloc((strlen(ssh_key_path) + 1) * sizeof(char));
	strcpy(remote_host->ssh_key_path, ssh_key_path);
	remote_host->ssh_username = malloc((strlen(ssh_username) + 1) * sizeof(char));
	strcpy(remote_host->ssh_username, ssh_username);
	remote_host->ssh_password = malloc((strlen(ssh_password) + 1) * sizeof(char));
	strcpy(remote_host->ssh_password, ssh_password);

	if (parent)
		parent->next_remote_host = remote_host;

	return (remote_host);
}

long get_remote_host_count(struct RemoteHost* remote_hosts)
{
	long remote_host_count = 0;

	struct RemoteHost* current_remote_host = remote_hosts;

	while (current_remote_host)
	{
		remote_host_count += 1;

		current_remote_host = current_remote_host->next_remote_host;
	}

	return (remote_host_count);
}

struct CommandDetails* create_new_command(struct CommandDetails* parent, long command_id)
{
	struct CommandDetails* new_command = malloc(sizeof(struct CommandDetails));
	memset(new_command, 0, sizeof(struct CommandDetails));
	new_command->stderr_response_id = -1;
	new_command->stdout_response_id = -1;
	new_command->command_id = command_id;

	if (parent)
		parent->next_command = new_command;

	return (new_command);
}

static int wait_for_socket(int* ssh_socket, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&fd);

    FD_SET(*ssh_socket, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions(session);


    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;

    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;

    rc = select((*ssh_socket) + 1, readfd, writefd, NULL, &timeout);

    return (rc);
}

void ssh_disconnect(LIBSSH2_SESSION* session, int* socket)
{
	libssh2_session_disconnect(session, "Disconnected...");
	libssh2_session_free(session);

#ifdef _WIN32
	closesocket(*socket);
	WSACleanup();
#else
	close(*socket);
#endif

	libssh2_exit();
}

unsigned char ssh_connect(const char* ssh_host, const char* ssh_username, const char* ssh_password, const char* ssh_key_path, int* ssh_socket, LIBSSH2_SESSION** ssh_session)
{
    short int active_authentication_type = 0;
    struct sockaddr_in socket_address_in;
    memset(&socket_address_in, 0, sizeof(struct sockaddr_in));
    char* user_authentication_methods = NULL;

    MUTEX_LOCK(&mutex);
    MUTEX_UNLOCK(&mutex);
	*ssh_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	
    socket_address_in.sin_family = AF_INET;
    socket_address_in.sin_port = htons(22);
    socket_address_in.sin_addr.s_addr = inet_addr(ssh_host);

    if (connect(*ssh_socket, (struct sockaddr*) &socket_address_in, (socklen_t) sizeof(struct sockaddr_in)) != 0)
    {
    	DEBUG_OUTPUT(stderr, "SSH ERROR: Failed to connect to host\n");
        return (SSH_ERROR_FAILED_TO_CONNECT);
    }

    *ssh_session = libssh2_session_init();

    /* We have to disable timeout, as processing in practice may take some time */
    libssh2_session_set_timeout(*ssh_session, 0);

    /* Setting SSH compression to speed up big data transfers */
    /* _SC - server-client, _CS - client-server */
    libssh2_session_method_pref(*ssh_session, LIBSSH2_METHOD_COMP_SC, "zlib");

    if (libssh2_session_handshake(*ssh_session, *ssh_socket))
    {
    	DEBUG_OUTPUT(stderr, "SSH ERROR: Failure establishing SSH session\n");
        return (SSH_ERROR_FAILURE_ESTABLISHING_SESSION);
    }

    user_authentication_methods = libssh2_userauth_list(*ssh_session, ssh_username, (unsigned int) strlen(ssh_username));

    DEBUG_OUTPUT(stdout, "Authentication methods: %s\n", user_authentication_methods);

    if (strstr(user_authentication_methods, "password") != NULL)
    	active_authentication_type |= 1;

    if (strstr(user_authentication_methods, "publickey") != NULL)
    	active_authentication_type |= 2;

    if ((active_authentication_type & 1) && strlen(ssh_password) != 0)
    	active_authentication_type = 1;

	if ((active_authentication_type & 2) && strlen(ssh_key_path) != 0)
		active_authentication_type = 2;

    if (active_authentication_type & 1)
    {
        if (libssh2_userauth_password(*ssh_session, ssh_username, ssh_password))
        {
        	DEBUG_OUTPUT(stderr, "SSH ERROR: Authentication by password failed!\n");
            ssh_disconnect(*ssh_session, ssh_socket);
            return (SSH_ERROR_PASSWORD_AUTHENTICATION_FAILED);
        }
        else
        {
        	DEBUG_OUTPUT(stdout, "Authentication by password succeeded.\n");
        }
    }
    else if (active_authentication_type & 2)
    {
        if (libssh2_userauth_publickey_fromfile(*ssh_session, ssh_username, NULL, ssh_key_path, ""))
        {
        	DEBUG_OUTPUT(stderr, "SSH ERROR: Authentication by public key failed!\n");
            ssh_disconnect(*ssh_session, ssh_socket);
            return (SSH_ERROR_KEY_AUTHENTICATION_FAILED);
        }
        else
        {
        	DEBUG_OUTPUT(stdout, "Authentication by public key succeeded.\n");
        }
    }
    else
    {
    	DEBUG_OUTPUT(stderr, "SSH ERROR: No supported authentication methods found!\n");
        ssh_disconnect(*ssh_session, ssh_socket);
        return (SSH_ERROR_NO_SUPPORTED_AUTHENTICATION_METHODS_FOUND);
    }

    return (SSH_SUCCESS);
}

void ssh_execute_commands(LIBSSH2_SESSION* ssh_session, int* ssh_socket, struct CommandDetails* commands, struct Command* command_dictionary, struct Response** responses_dictionary)
{
    LIBSSH2_CHANNEL* ssh_channel = NULL;
	struct CommandDetails* current_command = NULL;
	int command_response = 0;
	long response_bytes = 0;
	int exit_code = 0;
	char* exit_signal = (char *) "none";
	char* response_buffer = malloc(1024 * sizeof(char));
	char* response_string_stdout = NULL;
	char* response_string_stderr = NULL;

	current_command = commands;

	while (current_command)
	{
		DEBUG_OUTPUT(stdout, "Executing command: %ld\n", current_command->command_id);

		while ((ssh_channel = libssh2_channel_open_session(ssh_session)) == NULL && libssh2_session_last_error(ssh_session,NULL,NULL,0) == LIBSSH2_ERROR_EAGAIN)
			wait_for_socket(ssh_socket, ssh_session);

		if (ssh_channel == NULL)
		{
			DEBUG_OUTPUT(stderr,"SSH ERROR: Failed to initiate SSH session.\n");
			return;
		}

		while ((command_response = libssh2_channel_exec(ssh_channel, get_command_from_dictionary_by_id(command_dictionary, current_command->command_id))) == LIBSSH2_ERROR_EAGAIN)
			wait_for_socket(ssh_socket, ssh_session);

		if (command_response != 0)
		{
			DEBUG_OUTPUT(stderr,"SSH ERROR: Error during command execution: %d\n", command_response);
			return;
		}

		response_string_stdout = malloc(1 * sizeof(char));
		memset(response_string_stdout, 0, 1 * sizeof(char));
		while (1)
		{
			do
			{
				memset(response_buffer, 0, 1024 * sizeof(char));
				response_bytes = libssh2_channel_read(ssh_channel, response_buffer, 1024 * sizeof(char));
				form_response_string(&response_string_stdout, response_buffer, (unsigned long) response_bytes);
			}
			while (response_bytes > 0);

			if (response_bytes == LIBSSH2_ERROR_EAGAIN)
				wait_for_socket(ssh_socket, ssh_session);
			else
				break;
		}

		if (strlen(response_string_stdout))
			current_command->stdout_response_id = record_response_in_dictionary(responses_dictionary, response_string_stdout, response_type_stdout);

		response_string_stderr = malloc(1 * sizeof(char));
		memset(response_string_stderr, 0, 1 * sizeof(char));
		while (1)
		{
			do
			{
				memset(response_buffer, 0, 1024 * sizeof(char));
				response_bytes = libssh2_channel_read_stderr(ssh_channel, response_buffer, 1024 * sizeof(char));
				form_response_string(&response_string_stderr, response_buffer, (unsigned long) response_bytes);
			}
			while (response_bytes > 0);

			if (response_bytes == LIBSSH2_ERROR_EAGAIN)
				wait_for_socket(ssh_socket, ssh_session);
			else
				break;
		}

		if (strlen(response_string_stderr))
			current_command->stderr_response_id = record_response_in_dictionary(responses_dictionary, response_string_stderr, response_type_stderr);

		exit_code = 127; /* FIXME: Explain this??? */

		while ((command_response = libssh2_channel_close(ssh_channel)) == LIBSSH2_ERROR_EAGAIN)
			wait_for_socket(ssh_socket, ssh_session);

		if (command_response == 0)
		{
			exit_code = libssh2_channel_get_exit_status(ssh_channel);
			libssh2_channel_get_exit_signal(ssh_channel, &exit_signal, NULL, NULL, NULL, NULL, NULL);
		}

		if (exit_signal)
		{
			DEBUG_OUTPUT(stdout, "Got signal: %s\n", exit_signal);
		}
		else
		{
			DEBUG_OUTPUT(stdout, "Exit code: %d\n", exit_code);
		}

		libssh2_channel_free(ssh_channel);

		ssh_channel = NULL;

		current_command = current_command->next_command;
	}

	free(response_buffer);
}

struct Command* create_new_dictionary_command(struct Command* parent, const char* command_string)
{
	struct Command* new_command = malloc(sizeof(struct Command));
	new_command->command = malloc((strlen(command_string) + 1) * sizeof(char));
	strcpy(new_command->command, command_string);
	new_command->next_command = NULL;

	if (parent)
		parent->next_command = new_command;

	return (new_command);
}

void* remote_host_operation(void* host_data)
{
#ifdef _WIN32
	thread_setup();
#endif
	int socket = 0;
	LIBSSH2_SESSION* session = NULL;
	unsigned char ssh_response = 0;

	DEBUG_OUTPUT_MUTEX_LOCK(&mutex);
	DEBUG_OUTPUT(stdout, "Processing host: %s\n", ((struct RemoteHostData*) host_data)->remote_host->host_name);
	DEBUG_OUTPUT_MUTEX_UNLOCK(&mutex);

	const char* remote_host_ip = hostname_to_ip(((struct RemoteHostData*) host_data)->remote_host->host_name);

	if (
		(
			ssh_response = ssh_connect(
				remote_host_ip,
				((struct RemoteHostData*) host_data)->remote_host->ssh_username,
				((struct RemoteHostData*) host_data)->remote_host->ssh_password,
				((struct RemoteHostData*) host_data)->remote_host->ssh_key_path,
				&socket,
				&session
			)
		) != SSH_SUCCESS
	)
	{
		MUTEX_LOCK(&mutex);
		((struct RemoteHostData*) host_data)->remote_host->error_code = ssh_response;
		MUTEX_UNLOCK(&mutex);
	}
	else
	{
		ssh_execute_commands(session, &socket, ((struct RemoteHostData*) host_data)->remote_host->commands, ((struct RemoteHostData*) host_data)->commands_dictionary, ((struct RemoteHostData*) host_data)->responses_dictionary);
		ssh_disconnect(session, &socket);
	}

	MUTEX_LOCK(&mutex);
	((struct RemoteHostData*) host_data)->remote_host->status = processing_finished;
	MUTEX_UNLOCK(&mutex);

	return (NULL);
}

void* execute_remote_host_operations(void* ssh_data)
{
	struct RemoteHost* current_remote_host = ((struct SSHData*) ssh_data)->remote_hosts;

	long threads_running = 0;
	long processed_hosts = 0;
	long hosts_available = get_remote_host_count(((struct SSHData*) ssh_data)->remote_hosts);

#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 0), &wsadata);
#endif

	pthread_mutex_init(&mutex, NULL);
	libssh2_init(0);

#ifdef _WIN32
	thread_setup();
#endif
	
	while (1)
	{
		pthread_t thread_handle;

		MUTEX_LOCK(&mutex);
		if (current_remote_host->status == in_queue && THREADS_ALLOWED > threads_running)
		{
			current_remote_host->status = processing;
			DEBUG_OUTPUT(stdout, "Found queued host to process: %s\n", current_remote_host->host_name);
			struct RemoteHostData* remote_host_data = malloc(sizeof(struct RemoteHostData));
			remote_host_data->remote_host = current_remote_host;
			remote_host_data->commands_dictionary = ((struct SSHData*) ssh_data)->commands_dictionary;
			remote_host_data->responses_dictionary = ((struct SSHData*) ssh_data)->responses_dictionary;

			MUTEX_UNLOCK(&mutex);

#ifndef DEBUG_NON_THREADED
			pthread_create(&thread_handle, NULL, remote_host_operation, (void*) remote_host_data);
#else
			remote_host_operation((void*) remote_host_data);
#endif

			threads_running += 1;
		}
		else if (current_remote_host->status == processing_finished)
		{
			current_remote_host->status = ready;
			DEBUG_OUTPUT(stdout, "Finished processing host: %s\n", current_remote_host->host_name);
			MUTEX_UNLOCK(&mutex);
			threads_running -= 1;
			processed_hosts += 1;
		}
		else
			MUTEX_UNLOCK(&mutex);

		if (processed_hosts == hosts_available)
			break;

		MUTEX_LOCK(&mutex);
		if (current_remote_host->next_remote_host)
			current_remote_host = current_remote_host->next_remote_host;
		else
			current_remote_host = ((struct SSHData*) ssh_data)->remote_hosts;
		MUTEX_UNLOCK(&mutex);

#ifndef _WIN32
		usleep(1000);
#else
		Sleep(1);
#endif
	}

	pthread_mutex_destroy(&mutex);
#ifdef _WIN32
	thread_cleanup();
#endif

	return (NULL);
}

void* check_remote_host_operations_status(long process_handle, long* finished_processes, long* active_processes, long* processes_overall)
{
	MUTEX_LOCK(&mutex);

	struct RemoteHost* current_remote_host = ((struct SSHData*) process_handle)->remote_hosts;

	*processes_overall = get_remote_host_count(((struct SSHData*) process_handle)->remote_hosts);

	while (current_remote_host)
	{
		if (current_remote_host->status == in_queue)
		{
			DEBUG_OUTPUT(stdout, "Found queued host: %s\n", current_remote_host->host_name);
			*active_processes += 1;
		}
		else if (current_remote_host->status == ready)
		{
			DEBUG_OUTPUT(stdout, "Finished processing host: %s\n", current_remote_host->host_name);
			*finished_processes += 1;
		}

		current_remote_host = current_remote_host->next_remote_host;
	}

	MUTEX_UNLOCK(&mutex);

	return (NULL);
}

const char* get_command_from_dictionary_by_id(struct Command* dictionary_commands, long command_id)
{
	struct Command* current_dictionary_command = dictionary_commands;
	long command_id_iterator = 0;

	while (current_dictionary_command)
	{
		if (command_id_iterator == command_id)
			return (current_dictionary_command->command);

		command_id_iterator += 1;

		current_dictionary_command = current_dictionary_command->next_command;
	}

	return (NULL);
}

long record_response_in_dictionary(struct Response** responses, char* response, enum ResponseType response_type)
{
	MUTEX_LOCK(&mutex);

	struct Response* current_response = *responses;
	struct Response* new_response = NULL;
	long dictionary_position = 0;

	while (current_response)
	{
		if (strcmp(current_response->response, response) == 0 && current_response->type == response_type)
		{
			MUTEX_UNLOCK(&mutex);
			return (dictionary_position);
		}

		dictionary_position += 1;

		if (current_response->next_response == NULL)
		{
			new_response = malloc(sizeof(struct Response));
			new_response->response = response;
			new_response->type = response_type;
			new_response->next_response = NULL;

			current_response->next_response = new_response;
			MUTEX_UNLOCK(&mutex);
			return (dictionary_position);
		}

		current_response = current_response->next_response;
	}

	new_response = malloc(sizeof(struct Response));
	new_response->response = response;
	new_response->type = response_type;
	new_response->next_response = NULL;
	*responses = new_response;

	MUTEX_UNLOCK(&mutex);
	return (dictionary_position);
}

void release_response_memory(struct Response** responses)
{
	struct Response* current_response = NULL;

	if (current_response == NULL)
		return;

	while (1)
	{
		current_response = *responses;
		if (current_response->next_response == NULL)
			break;

		while (current_response)
		{
			if (current_response->next_response == NULL)
				free(current_response);
			else
				break;
		}
	}

	free(*responses);
}
