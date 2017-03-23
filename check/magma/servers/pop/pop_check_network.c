
/**
 * @file /magma/check/magma/servers/pop/pop_check_network.c
 *
 * @brief Functions used to test POP connections over a network connection.
 *
 */

#include "magma_check.h"

/**
 * Calls 	client_read_line on a client until it reaches a period only line, returning the
 * 			number of messages in the inbox.
 *
 * @param 	client 	The client_t* to read from (which should be connected to a POP server).
 * @return 	true if a line containing a single period is found, false if not.
 */
bool_t check_pop_client_read_end(client_t *client) {

	while (client_read_line(client) > 0) {
		if (!st_cmp_cs_eq(&(client->line), NULLER(".\r\n"))) return true;
	}
	return false;
}

/**
 * Calls 	client_read_line on a client until it reaches a period only line, returning the
 * 			number of messages in the inbox.
 *
 * @param 	client 	The client_t* to read from (which should be connected to a POP server).
 * @param 	errmsg	The stringer_t* to which error messages will be printed in event of an error.
 * @return 	a uint32_t containing the number of messages in the inbox.
 */
uint64_t check_pop_client_read_list(client_t *client, stringer_t *errmsg) {

	placer_t fragment = pl_null();
	uint64_t counter = 0, sequence = 0;

	while (client_read_line(client) > 0) {

		if (pl_starts_with_char(client->line, '.')) {
			return counter;
		}
		else if (tok_get_st(&(client->line), ' ', 0, &fragment) >= 0 && !uint64_conv_pl(fragment, &sequence)) {
			if (sequence != counter) return 0;
		}

		counter++;
	}

	return 0;
}

bool_t check_pop_network_basic_sthread(stringer_t *errmsg, uint32_t port, bool_t secure) {

	uint64_t message_num;
	client_t *client = NULL;

	// Connect the client.
	if (!(client = client_connect("localhost", port)) || (secure && (client_secure(client) == -1)) ||
		!net_set_timeout(client->sockd, 20, 20) || client_read_line(client) <= 0 || client_status(client) != 1 ||
		st_cmp_cs_starts(&(client->line), NULLER("+OK"))) {

		st_sprint(errmsg, "Failed to connect with the POP server.");
		client_close(client);
		return false;
	}

	// Test the USER and PASS commands with incorrect credentials.
	else if (client_print(client, "USER princess\r\n") != 15 || client_read_line(client) <= 0 ||
		client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("+OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after USER.");
		client_close(client);
		return false;
	}
	else if (client_print(client, "PASS lavabit\r\n") != 14 || client_read_line(client) <= 0 ||
		client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("-ERR"))) {

		st_sprint(errmsg, "Failed to return an error state after PASS with incorrect credentials.");
		client_close(client);
		return false;
	}

	// Test the USER and PASS commands with correct credentials.
	else if (client_print(client, "USER princess\r\n") != 15 || client_read_line(client) <= 0 ||
		client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("+OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after USER.");
		client_close(client);
		return false;
	}
	else if (client_print(client, "PASS password\r\n") != 15 || client_read_line(client) <= 0 ||
		client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("+OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after USER.");
		client_close(client);
		return false;
	}

	// Test the LIST command.
	else if (client_print(client, "LIST\r\n") != 6 || !(message_num = check_pop_client_read_list(client, errmsg)) ||
		client_status(client) != 1) {

		if (!errmsg) st_sprint(errmsg, "Failed to return a successful state after LIST.");
		client_close(client);
		return false;
	}

	// Test the RETR command.
	else if (client_print(client, "RETR 1\r\n") != 8 || !check_pop_client_read_end(client) ||
		client_status(client) != 1) {

		st_sprint(errmsg, "Failed to return a successful state after RETR.");
		client_close(client);
		return false;
	}

	// Test the DELE command.
	else if (client_print(client, "DELE 1\r\n") != 8 || client_read_line(client) <= 0 || client_status(client) != 1 ||
		st_cmp_cs_starts(&(client->line), NULLER("+OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after DELE.");
		client_close(client);
		return false;
	}

	// Test the NOOP command.
	else if (client_print(client, "NOOP\r\n") != 6 || client_read_line(client) <= 0 || client_status(client) != 1 ||
		st_cmp_cs_starts(&(client->line), NULLER("+OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after NOOP.");
		client_close(client);
		return false;
	}

	// Test the LAST command.
	else if (client_print(client, "LAST 1\r\n") != 8 || client_read_line(client) <= 0 || client_status(client) != 1 ||
		st_cmp_cs_starts(&(client->line), NULLER("+OK")) || st_cmp_cs_ends(&(client->line),
		st_aprint_opts(MANAGED_T | CONTIGUOUS | STACK, "%lu\r\n", message_num))) {

		st_sprint(errmsg, "Failed to return a successful state after LAST.");
		client_close(client);
		return false;
	}

	// Test the RSET command.
	else if (client_print(client, "RSET\r\n") != 6 || client_read_line(client) <= 0 || client_status(client) != 1 ||
		st_cmp_cs_starts(&(client->line), NULLER("+OK")) || st_cmp_cs_ends(&(client->line), NULLER("were reset.\r\n"))) {

		st_sprint(errmsg, "Failed to return a successful state after RSET.");
		client_close(client);
		return false;
	}

	// Test the QUIT command.
	else if (client_print(client, "QUIT 1\r\n") <= 0 || client_read_line(client) <= 0 || client_status(client) != 1 ||
		st_cmp_cs_starts(&(client->line), NULLER("+OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after QUIT.");
		client_close(client);
		return false;
	}

	client_close(client);

	return true;
}
