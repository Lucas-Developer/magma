
/**
 * @file /magma/check/magma/servers/http/http_check_network.c
 *
 * @brief Functions used to test HTTP connections over a network connection.
 *
 */

#include "magma_check.h"

/**
 * @brief	Reads lines from client until an empty line is reached.
 * @param	client	A client_t* containing response lines to read.
 * @return	True if an empty line was reached, false otherwise.
 */
bool_t check_http_read_to_empty(client_t *client) {

	while (client_read_line(client) >= 0) {
		if (pl_length_get(client->line) == 2) return true; // contains only "\r\n"
	}
	return false;
}

/**
 * @brief 	Reads lines from client and returns the value of Content-Length.
 *
 * This function reads lines from the passed client until it finds the Content-Length
 * line, at which point it parses the value and returns it.
 *
 * @param 	client	A client_t containing the response of an HTTP request.
 * @return 	The value of Content-Length in the HTTP message header.
 */
uint32_t check_http_content_length_get(client_t *client, stringer_t *errmsg) {

	size_t location = 0;
	uint32_t content_length;
	placer_t cl_placer = pl_null();

	while (st_cmp_ci_starts(&(client->line), NULLER("Content-Length:")) != 0) client_read_line(client);

	if (!st_search_chr(&(client->line), ' ', &location)) {
		st_sprint(errmsg, "The Content-Length line was improperly formed.");
	}
	else if (pl_empty(cl_placer = pl_init(pl_data_get(client->line) + location, pl_length_get(client->line) - location))) {
		st_sprint(errmsg, "Failed to initialize content length placer.");
	}
	else if (!pl_inc(&cl_placer, pl_length_get(client->line) - location) || !(cl_placer.length = pl_length_get(cl_placer)-2)) {
		st_sprint(errmsg, "Failed to increment placer to location of content-length value.");
	}
	else if (!uint32_conv_st(&cl_placer, &content_length)) {
		st_sprint(errmsg, "Failed to convert the content-length string to a uint32");
	}

	return content_length;
}

/**
 * @brief 	Reads lines from the client until the end of message, checking if it matches content_length.
 *
 * @param 	client			A client_t* that should be connected to an HTTP server and have read to the beginning
 * 							of the body of a response.
 * @param	content_length	A unint32_t containing the expected size of the message body.
 * @return	True if the message body size matches content_length, false if not.
 */
bool_t check_http_content_length_test(client_t *client, uint32_t content_length, stringer_t *errmsg) {

	uint32_t total = 0;

	while (total < content_length) {
		total += client_read_line(client);
	}

	return (total == content_length);
}

bool_t check_http_network_basic_sthread(stringer_t *errmsg, uint32_t port, bool_t secure) {

	size_t content_length;
	client_t *client = NULL;

	// Test the connection.
	if (!(client = client_connect("localhost", port)) || (secure && (client_secure(client) == -1)) ||
		client_status(client) != 1) {

		st_sprint(errmsg, "Failed to connect with the HTTP server.");
		client_close(client);
		return false;
	}
	// Test submitting a GET request.
	else if (client_print(client, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n") != 35 || client_status(client) != 1 ||
		!(content_length = check_http_content_length_get(client, errmsg))) {

		if (!errmsg) st_sprint(errmsg, "Failed to return a valid GET response.");
		return false;
	}
	// Test the response.
	else if (check_http_content_length_test(client, content_length, errmsg)) {
		if (!errmsg) st_sprint(errmsg, "The content length and actual body length of the GET response did not match.");
		return false;
	}

	client_close(client);

	return true;
}
