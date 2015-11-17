#ifdef _WIN32
#include <Python.h>
#else
#include <python2.7/Python.h>
#endif

#include "definitions.h"
#include "functions.h"

#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>

static struct Response* response = NULL;

PyObject *start_remote_manager_wrapper(PyObject *self, PyObject *py_arguments)
{
	/* Variable declaration and initialization */
	pthread_t thread_handle;

	/* Python representation of provided arguments */
	/* Hosts */
	PyObject* py_hosts_list = NULL;
	Py_ssize_t hosts_list_iterator = 0;
	PyObject* py_current_host = NULL;
	Py_ssize_t host_count = 0;
	/* Hosts' commands */
	PyObject* py_host_command_data_list = NULL;
	Py_ssize_t host_command_data_list_iterator = 0;
	Py_ssize_t host_command_data_count = 0;
	/* Dictionary of commands */
	PyObject* py_dictionary_commands_list = NULL;
	Py_ssize_t dictionary_commands_list_iterator = 0;
	Py_ssize_t dictionary_command_count = 0;
	/* Handle to return back to Python code */
	PyObject* py_handle = NULL;

	/* C representation of provided arguments */
	struct RemoteHost* remote_hosts = NULL;
	struct RemoteHost* current_remote_host = NULL;
	struct CommandDetails* host_command_data = NULL;
	struct CommandDetails* current_host_command_data = NULL;
	struct Command* dictionary_commands = NULL;
	struct Command* current_dictionary_command = NULL;
	struct SSHData* ssh_data = NULL;

	/* Getting arguments to process */
	if (!PyArg_ParseTuple(py_arguments, "OO", &py_hosts_list, &py_dictionary_commands_list))
	{
		PyErr_SetString(PyExc_ValueError, "Incorrect arguments provided");
		return NULL;
	}

	/* Identify amount of hosts to process */
	host_count = PyObject_Length(py_hosts_list);

	/* Iterating through hosts and translating Python objects into C structs for processing */
	for (hosts_list_iterator = 0; hosts_list_iterator < host_count; hosts_list_iterator++)
	{
		/* Example of incoming data from Python code */

		/* Hosts list:
		 * [
		 * 	{
		 * 		'host': 'test.com',
		 * 		'commands': [0, 1],
		 * 		'role': 'ubuntu',
		 * 		'ssh_username': 'ubuntu',
		 * 		'ssh_password': 'test',
		 * 		'ssh_key_path': '/tmp/test.pem'
		 * 	}
		 * ]
		 *
		 * Commands dictionary:
		 * ['command_1', 'command_2'] */

		DEBUG_OUTPUT(
			stdout,
			"Fetched host from list: %s\n",
			PyString_AsString(
				PyObject_Str(
					PyObject_GetItem(
						py_hosts_list,
						PyInt_FromLong(hosts_list_iterator)
					)
				)
			)
		);

		/* Getting current host (dict) from hosts list */
		py_current_host = PyObject_GetItem(py_hosts_list, PyInt_FromLong(hosts_list_iterator));

		/* Getting host's command data (list of command ids) from dict */
		py_host_command_data_list = PyObject_GetItem(py_current_host, PyString_FromString("commands"));

		/* Getting command ids count from host's commands list */
		host_command_data_count = PyObject_Length(py_host_command_data_list);

		DEBUG_OUTPUT(stdout, "Host's command count: %ld\n", host_command_data_count);

		/* Resetting current host's commands before processing */
		current_host_command_data = NULL;

		/* Iterating over host's commands list */
		for (host_command_data_list_iterator = 0; host_command_data_list_iterator < host_command_data_count; host_command_data_list_iterator++)
		{
			DEBUG_OUTPUT(
				stdout,
				"Fetched command from list: %s\n",
				PyString_AsString(
					PyObject_GetItem(
						py_dictionary_commands_list,
						PyObject_GetItem(
							py_host_command_data_list,
							PyInt_FromLong(host_command_data_list_iterator)
						)
					)
				)
			);

			/* Create new host's command entity (C struct) */
			current_host_command_data = create_new_command(
				current_host_command_data,
				PyInt_AsLong(
					PyObject_GetItem(
						py_host_command_data_list,
						PyInt_FromLong(host_command_data_list_iterator)
					)
				)
			);

			if (host_command_data_list_iterator == 0)
				host_command_data = current_host_command_data;
		}

		/* Create new host entity (C struct) */
		current_remote_host = create_new_remote_host(
			current_remote_host,
			host_command_data,
			PyString_AsString(
				PyObject_GetItem(py_current_host, PyString_FromString("host"))
			),
			PyString_AsString(
				PyObject_GetItem(py_current_host, PyString_FromString("ssh_username"))
			),
			PyString_AsString(
				PyObject_GetItem(py_current_host, PyString_FromString("ssh_key_path"))
			),
			PyString_AsString(
				PyObject_GetItem(py_current_host, PyString_FromString("ssh_password"))
			)
		);

		if (hosts_list_iterator == 0)
			remote_hosts = current_remote_host;
	}

	/* Getting command count from commands dictionary */
	dictionary_command_count = PyObject_Length(py_dictionary_commands_list);

	DEBUG_OUTPUT(stdout, "Command count (commands dictionary): %ld\n", host_command_data_count);

	/* Iterating over dictionary commands and representing then as C structs */
	for (dictionary_commands_list_iterator = 0; dictionary_commands_list_iterator < dictionary_command_count; dictionary_commands_list_iterator++)
	{
		DEBUG_OUTPUT(
			stdout,
			"Fetched command from dictionary: %s\n",
			PyString_AsString(
				PyObject_GetItem(
					py_dictionary_commands_list,
					PyInt_FromLong(dictionary_commands_list_iterator)
				)
			)
		);

		current_dictionary_command = create_new_dictionary_command(
			current_dictionary_command,
			PyString_AsString(
				PyObject_GetItem(
					py_dictionary_commands_list,
					PyInt_FromLong(dictionary_commands_list_iterator)
				)
			)
		);

		if (dictionary_commands_list_iterator == 0)
			dictionary_commands = current_dictionary_command;
	}

	/* Passing processed arguments to new thread */
	ssh_data = malloc(sizeof(struct SSHData));
	ssh_data->commands_dictionary = dictionary_commands;
	ssh_data->remote_hosts = remote_hosts;
	ssh_data->responses_dictionary = &response;

	/* Starting processing thread */
	pthread_create(&thread_handle, NULL, execute_remote_host_operations, (void*) ssh_data);

	/* Returning back process handle */
	py_handle = PyInt_FromLong((long) ssh_data);
	Py_INCREF(py_handle);
	return (py_handle);
}

PyObject *check_status_wrapper(PyObject *self, PyObject *py_arguments)
{
	PyObject* py_response = NULL;
	long manager_handle = 0;
	long finished_processes = 0;
	long active_processes = 0;
	long processes_overall = 0;

	if (!PyArg_ParseTuple(py_arguments, "l", &manager_handle))
		return NULL;

	check_remote_host_operations_status(manager_handle, &finished_processes, &active_processes, &processes_overall);

	py_response = PyTuple_Pack(3, PyInt_FromLong(finished_processes), PyInt_FromLong(active_processes), PyInt_FromLong(processes_overall));

	Py_INCREF(py_response);
	return (py_response);
}

PyObject *get_response_wrapper(PyObject *self, PyObject *py_arguments)
{
	/* import threaded_remote_manager */
	/* handle = threaded_remote_manager.start_remote_manager([{'host': 'lb11.***.com', 'commands': [0, 1], 'role': '...', 'ssh_username': '...', 'ssh_password': '', 'ssh_key_path': '/Users/.../....pem'}, {'host': 'app11.....com', 'commands': [0, 1], 'role': '...', 'ssh_username': '...', 'ssh_password': '', 'ssh_key_path': '/Users/.../....pem'}], ['ls -alh', 'uname -a']) */
	/* threaded_remote_manager.check_status(handle) */
	/* threaded_remote_manager.get_response(handle) */

	long manager_handle = 0;
	struct RemoteHost* current_remote_host = NULL;
	struct Response* current_response_in_dictionary = NULL;
	struct CommandDetails* current_remote_host_command = NULL;

	if (!PyArg_ParseTuple(py_arguments, "l", &manager_handle))
		return NULL;

	struct RemoteHost* remote_hosts = ((struct SSHData*) manager_handle)->remote_hosts;

	/* {
	 * 		'hosts': [
	 * 			{
	 * 				'host': 'test.com',
	 * 				'commands': [(0, 0, 0), (1, 0, 1)],
	 * 				'error_code': 0
	 * 			}
	 * 		],
	 *
	 * 		responses_dictionary: ['response_1', 'response_2']
	 */

	PyObject* py_response = NULL;
	PyObject* py_responses_list = NULL;
	PyObject* py_remote_hosts_response_list = NULL;
	PyObject* py_remote_host_response_dict = NULL;
	PyObject* py_remote_host_commands_list = NULL;

	py_response = PyDict_New();
	py_responses_list = PyList_New(0);
	py_remote_hosts_response_list = PyList_New(0);

	current_remote_host = remote_hosts;

	while (current_remote_host)
	{
		py_remote_host_response_dict = PyDict_New();

		PyDict_SetItem(py_remote_host_response_dict, PyString_FromString("host"), PyString_FromString(current_remote_host->host_name));

		py_remote_host_commands_list = PyList_New(0);

		current_remote_host_command = current_remote_host->commands;

		while (current_remote_host_command)
		{
			PyList_Append(
				py_remote_host_commands_list,
				PyTuple_Pack(
					3,
					PyInt_FromLong(current_remote_host_command->command_id),
					PyInt_FromLong(current_remote_host_command->stdout_response_id),
					PyInt_FromLong(current_remote_host_command->stderr_response_id)
				)
			);

			current_remote_host_command = current_remote_host_command->next_command;
		}

		PyDict_SetItem(py_remote_host_response_dict, PyString_FromString("commands"), py_remote_host_commands_list);

		PyDict_SetItem(py_remote_host_response_dict, PyString_FromString("error_code"), PyInt_FromLong(current_remote_host->error_code));

		current_remote_host = current_remote_host->next_remote_host;

		PyList_Append(py_remote_hosts_response_list, py_remote_host_response_dict);
	}

	PyDict_SetItem(py_response, PyString_FromString("hosts"), py_remote_hosts_response_list);

	current_response_in_dictionary = *((struct SSHData*) manager_handle)->responses_dictionary;

	while (current_response_in_dictionary)
	{
		PyList_Append(py_responses_list, PyString_FromString(current_response_in_dictionary->response));

		current_response_in_dictionary = current_response_in_dictionary->next_response;
	}

	PyDict_SetItem(py_response, PyString_FromString("responses_dictionary"), py_responses_list);

	release_response_memory(&response);
	response = NULL;

	Py_INCREF(py_response);
	return (py_response);
}

static PyMethodDef threaded_remote_managerMethods[] =
{
	{
		"start_remote_manager",
		start_remote_manager_wrapper,
		METH_KEYWORDS,
		""
	},
	{
		"check_status",
		check_status_wrapper,
		METH_KEYWORDS,
		""
	},
	{
		"get_response",
		get_response_wrapper,
		METH_KEYWORDS,
		""
	},
	{
		NULL,
		NULL,
		0,
		NULL
	}
};

DL_EXPORT(void) initthreaded_remote_manager(void)
{
	Py_InitModule("threaded_remote_manager", threaded_remote_managerMethods);
}
