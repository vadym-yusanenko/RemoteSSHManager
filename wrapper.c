/*
 * wrapper.c
 *
 *  Created on: Nov 22, 2016
 *      Author: vadimyusanenko
 */

#ifdef _WIN32
	#include <Python.h>
#else
	#include <python2.7/Python.h>
#endif

#define DEBUG_OUTPUT fprintf

#include <netinet/in.h>
#include <libssh2.h>
#include <netdb.h>
#include <arpa/inet.h>


void form_response_string(char** response_string, char* response_chunk, long response_chunk_size)
{
	unsigned long response_size = strlen(*response_string);

	*response_string = realloc(*response_string, response_size = (response_size + response_chunk_size + (1 * sizeof(char))));

	memcpy(*response_string + strlen(*response_string), response_chunk, response_chunk_size);
	memset(*response_string + response_size - (1 * sizeof(char)), 0, (1 * sizeof(char)));
}


const char* hostname_to_ip(const char* hostname)
{
	struct in_addr internet_address;
	memset(&internet_address, 0, sizeof(internet_address));

	struct addrinfo* result = NULL;

	getaddrinfo(hostname, NULL, NULL, &result);

	if (result == NULL || result->ai_addr == NULL)
		return ("unknown");

	internet_address.s_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr;

	freeaddrinfo(result);

	return (inet_ntoa(internet_address));
}


PyObject* execute_ssh_instructions(PyObject* self, PyObject* args, PyObject* kwargs)
{
	Py_Initialize();
	/* TODO: Not thread safe!!! */
	libssh2_init(0);

	// import remote_ssh_manager; remote_ssh_manager.execute_ssh_instructions('app31.mojosells.com', 'ubuntu', None, '/Users/vadimyusanenko/mojo-app-server.pem', ['ls -alh', 'ifconfig', 'uname -a'])

	static char* arguments[] = {"ssh_host", "ssh_username", "ssh_password", "ssh_key_path", "ssh_commands", NULL};

	char* ssh_host;
	char* ssh_username;
	char* ssh_password;
	char* ssh_key_path;
	PyListObject* command_list;
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sszzO", arguments, &ssh_host, &ssh_username, &ssh_password, &ssh_key_path, &command_list))
	{
		PyErr_SetString(PyExc_Exception, "Invalid argument(s) provided");
		return (PyObject*) NULL;
	}

	DEBUG_OUTPUT(stdout, "=> Creating socket...\n");
	int ssh_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	DEBUG_OUTPUT(stdout, "\tDONE\n");

	DEBUG_OUTPUT(stdout, "=> Resolving IP by domain...\n");
	const char* ip = hostname_to_ip(ssh_host);
	if (strcmp(ip, "unknown") == 0)
	{
		PyErr_SetString(PyExc_Exception, "Invalid domain provided");
		return (PyObject*) NULL;
	}
	DEBUG_OUTPUT(stdout, "\t%s => %s\n", ssh_host, ip);

	struct sockaddr_in socket_address_in;
	memset(&socket_address_in, 0, sizeof(struct sockaddr_in));
	socket_address_in.sin_family = AF_INET;
	socket_address_in.sin_port = htons(22);
	socket_address_in.sin_addr.s_addr = inet_addr(ip);

	DEBUG_OUTPUT(stdout, "=> Connecting to server...\n");
	if (connect(ssh_socket, (struct sockaddr*) &socket_address_in, (socklen_t) sizeof(struct sockaddr_in)) != 0)
	{
		PyErr_SetString(PyExc_Exception, "Failed to connect to host");
		return (PyObject*) NULL;
	}
	DEBUG_OUTPUT(stdout, "\tDONE\n");

	DEBUG_OUTPUT(stdout, "=> Initializing libssh2 session...\n");
	LIBSSH2_SESSION* ssh_session = libssh2_session_init();
	DEBUG_OUTPUT(stdout, "\tDONE\n");

	DEBUG_OUTPUT(stdout, "=> Enabling blocking mode...\n");
	libssh2_session_set_blocking(ssh_session, 1);
	DEBUG_OUTPUT(stdout, "\tDONE\n");

	DEBUG_OUTPUT(stdout, "=> Setting maximum supported timeout for SSH session...\n");
    /* We have to disable timeout, as processing in practice may take some time */
    libssh2_session_set_timeout(ssh_session, 0);
    DEBUG_OUTPUT(stdout, "\tDONE\n");

    DEBUG_OUTPUT(stdout, "=> Enabling SSH protocol compression...\n");
    /* Setting SSH compression to speed up big data transfers */
    /* _SC - server-client, _CS - client-server */
    libssh2_session_method_pref(ssh_session, LIBSSH2_METHOD_COMP_SC, "zlib");
    DEBUG_OUTPUT(stdout, "\tDONE\n");

    DEBUG_OUTPUT(stdout, "=> Performing SSH handshake...\n");
    if (libssh2_session_handshake(ssh_session, ssh_socket))
    {
    	PyErr_SetString(PyExc_Exception, "Failure establishing SSH session");
    	return (PyObject*) NULL;
    }
    DEBUG_OUTPUT(stdout, "\tDONE\n");

    DEBUG_OUTPUT(stdout, "=> Getting available authentication methods...\n");
    const char* user_authentication_methods = libssh2_userauth_list(ssh_session, ssh_username, (unsigned int) strlen(ssh_username));
    DEBUG_OUTPUT(stdout, "\t%s\n", user_authentication_methods);

	if (strstr(user_authentication_methods, "password") && strlen(ssh_password) != 0)
	{
		DEBUG_OUTPUT(stdout, "=> Authenticating with password...\n");
		if (libssh2_userauth_password(ssh_session, ssh_username, ssh_password))
		{
			PyErr_SetString(PyExc_Exception, "Authentication by password failed");
			return (PyObject*) NULL;
		}
		DEBUG_OUTPUT(stdout, "\tDONE\n");
	}
	else if (strstr(user_authentication_methods, "publickey") && strlen(ssh_key_path) != 0)
	{
		DEBUG_OUTPUT(stdout, "=> Authenticating with public key...\n");
		if (libssh2_userauth_publickey_fromfile(ssh_session, ssh_username, NULL, ssh_key_path, ""))
		{
			PyErr_SetString(PyExc_Exception, "Authentication by public key failed");
			return (PyObject*) NULL;
		}
		DEBUG_OUTPUT(stdout, "\tDONE\n");
	}
	else
	{
		PyErr_SetString(PyExc_Exception, "No supported authentication methods found");
		return (PyObject*) NULL;
	}

	unsigned char command_count = PyList_Size(command_list);

	PyTupleObject* py_response = PyTuple_New(command_count);

	for (unsigned char i = 0; i < command_count; i++)
	{
		PyTupleObject* py_command_response = PyTuple_New(2);

		DEBUG_OUTPUT(stdout, "=> Command: %s\n", PyString_AsString(PyList_GetItem(command_list, i)));

		LIBSSH2_CHANNEL* ssh_channel = libssh2_channel_open_session(ssh_session);

		if (ssh_channel == NULL)
		{
			PyErr_SetString(PyExc_Exception, "Failed to initiate SSH session");
			return (PyObject*) NULL;
		}

		libssh2_channel_set_blocking(ssh_channel, 1);

		DEBUG_OUTPUT(stdout, "=> Executing...\n");
		int command_response = libssh2_channel_exec(ssh_channel, PyString_AsString(PyList_GetItem(command_list, i)));
		if (command_response != 0)
		{
			/* continue? */
			PyErr_SetString(PyExc_Exception, "Error during command execution");
			return (PyObject*) NULL;
		}
		DEBUG_OUTPUT(stdout, "\tDONE\n");

		long response_bytes = 0;

		char* response_buffer = malloc(1024 * sizeof(char));

		DEBUG_OUTPUT(stdout, "=> Reading stdout...\n");
		char* response_string_stdout = malloc(1024 * sizeof(char));
		memset(response_string_stdout, 0, 1024 * sizeof(char));
		do
		{
			memset(response_buffer, 0, 1024 * sizeof(char));
			response_bytes = libssh2_channel_read(ssh_channel, response_buffer, 1024 * sizeof(char));
			form_response_string(&response_string_stdout, response_buffer, response_bytes);
		}
		while (response_bytes > 0);
		DEBUG_OUTPUT(stdout, "\tOUTPUT: %s\n", response_string_stdout);

		PyTuple_SetItem(py_command_response, 0, PyString_FromString(response_string_stdout));
		free(response_string_stdout);

		DEBUG_OUTPUT(stdout, "=> Reading stderr...\n");
		char* response_string_stderr = malloc(1024 * sizeof(char));
		memset(response_string_stderr, 0, 1024 * sizeof(char));
		do
		{
			memset(response_buffer, 0, 1024 * sizeof(char));
			response_bytes = libssh2_channel_read_stderr(ssh_channel, response_buffer, 1024 * sizeof(char));
			form_response_string(&response_string_stderr, response_buffer, (unsigned long) response_bytes);
		}
		while (response_bytes > 0);
		DEBUG_OUTPUT(stdout, "\tOUTPUT: %s\n", response_string_stdout);

		PyTuple_SetItem(py_command_response, 1, PyString_FromString(response_string_stderr));
		free(response_string_stderr);

		/* FIXME: Explain this??? */
		int exit_code = 127;

		command_response = libssh2_channel_close(ssh_channel);

		char* exit_signal = (char *) "none";
		if (command_response == 0)
		{
			exit_code = libssh2_channel_get_exit_status(ssh_channel);
			libssh2_channel_get_exit_signal(ssh_channel, &exit_signal, NULL, NULL, NULL, NULL, NULL);
		}

		if (exit_signal)
			DEBUG_OUTPUT(stdout, "Got signal: %s\n", exit_signal);
		else
			DEBUG_OUTPUT(stdout, "Exit code: %d\n", exit_code);

		libssh2_channel_free(ssh_channel);

		ssh_channel = NULL;

		free(response_buffer);

		PyTuple_SetItem(py_response, i, py_command_response);
	}

    return Py_BuildValue("O", py_response);
}

static PyMethodDef remote_ssh_manager_methods[] = {
    /* The cast of the function is necessary since PyCFunction values
     * only take two PyObject* parameters, and our function with key-value
     * arguments takes three.
     */
    {"execute_ssh_instructions", (PyCFunction) execute_ssh_instructions, METH_VARARGS|METH_KEYWORDS},
    {NULL,  NULL}
};

void initremote_ssh_manager()
{
  /* Create the module and add the functions */
  Py_InitModule("remote_ssh_manager", remote_ssh_manager_methods);
}
