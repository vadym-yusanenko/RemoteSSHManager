#include "definitions.h"
#include "functions.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

int main(int argc, char** argv)
{
#ifdef DEBUG_NON_THREADED
	printf("WARNING: This build is non-threaded: all logic will be executed synchronously.\n\n");
#endif

#ifdef _WIN32
	thread_setup();
#endif

	pthread_t thread_handle;

	struct RemoteHost* remote_hosts = NULL;

	struct CommandDetails* host_1_command_data = NULL;
	struct CommandDetails* host_2_command_data = NULL;

	struct Command* dictionary_commands = NULL;

	struct SSHData* ssh_data = NULL;

	host_1_command_data = create_new_command(NULL, 0);

	create_new_command(host_1_command_data, 1);

	remote_hosts = create_new_remote_host(
		NULL,
		host_1_command_data,
		"***",
		"***",
		"***",
		""
	);

	host_2_command_data = create_new_command(NULL, 0);

	create_new_command(host_2_command_data, 1);

	create_new_remote_host(
		remote_hosts,
		host_2_command_data,
		"***",
		"***",
		"***",
		""
	);

	dictionary_commands = create_new_dictionary_command(NULL, "sleep 5");

	create_new_dictionary_command(dictionary_commands, "uname -a");

	ssh_data = malloc(sizeof(struct SSHData));
	ssh_data->commands_dictionary = dictionary_commands;
	ssh_data->remote_hosts = remote_hosts;
	struct Response* response = NULL;
	ssh_data->responses_dictionary = &response;

#ifndef DEBUG_NON_THREADED
	pthread_create(&thread_handle, NULL, execute_remote_host_operations, (void*) ssh_data);
#else
	execute_remote_host_operations((void*) ssh_data);
#endif

#ifndef DEBUG_NON_THREADED
#ifndef _WIN32
	usleep(1000 * 1000 * 30);
#else
	Sleep(1000 * 30);
#endif
#endif

	release_response_memory(&response);

	return (0);
}
