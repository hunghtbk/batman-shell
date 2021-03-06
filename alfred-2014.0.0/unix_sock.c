/*
 * Copyright (C) 2012-2014 B.A.T.M.A.N. contributors:
 *
 * Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#include <errno.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "alfred.h"
#include "hash.h"
#include "packet.h"

int unix_sock_open_daemon(struct globals *globals, const char *path)
{
	struct sockaddr_un addr;

	unlink(path);

	globals->unix_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (globals->unix_sock < 0) {
		fprintf(stderr, "can't create unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strcpy(addr.sun_path, path);

	if (bind(globals->unix_sock, (struct sockaddr *)&addr,
		 sizeof(addr)) < 0) {
		fprintf(stderr, "can't bind unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	if (listen(globals->unix_sock, 10) < 0) {
		fprintf(stderr, "can't listen on unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

int unix_sock_open_client(struct globals *globals, const char *path)
{
	struct sockaddr_un addr;

	globals->unix_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (globals->unix_sock < 0) {
		fprintf(stderr, "can't create unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strcpy(addr.sun_path, path);

	if (connect(globals->unix_sock, (struct sockaddr *)&addr,
		    sizeof(addr)) < 0) {
		fprintf(stderr, "can't connect to unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}


static int unix_sock_add_data(struct globals *globals,
			      struct alfred_push_data_v0 *push,
			      int client_sock)
{
	struct alfred_data *data;
	struct dataset *dataset;
	int len, data_len, ret = -1;

	len = ntohs(push->header.length);

	if (len < (int)(sizeof(*push) + sizeof(push->header)))
		goto err;

	/* subtract rest of push header */
	len -= sizeof(*push) - sizeof(push->header);

	if (len < (int)(sizeof(*data)))
		goto err;

	data = push->data;
	data_len = ntohs(data->header.length);
	memcpy(data->source, &globals->hwaddr, sizeof(globals->hwaddr));

	if ((int)(data_len + sizeof(*data)) > len)
		goto err;

	dataset = hash_find(globals->data_hash, data);
	if (!dataset) {
		dataset = malloc(sizeof(*dataset));
		if (!dataset)
			goto err;

		dataset->buf = NULL;

		memcpy(&dataset->data, data, sizeof(*data));
		if (hash_add(globals->data_hash, dataset)) {
			free(dataset);
			goto err;
		}
	}
	dataset->data_source = SOURCE_LOCAL;
	clock_gettime(CLOCK_MONOTONIC, &dataset->last_seen);

	/* free old buffer */
	free(dataset->buf);

	dataset->buf = malloc(data_len);
	/* that's not good */
	if (!dataset->buf)
		goto err;

	dataset->data.header.length = data_len;
	memcpy(dataset->buf, data->data, data_len);

	ret = 0;
err:
	close(client_sock);
	return ret;
}


static int unix_sock_req_data_reply(struct globals *globals, int client_sock,
				    uint16_t id, uint8_t requested_type)
{
	int len;
	struct alfred_push_data_v0 *push;
	struct hash_it_t *hashit = NULL;
	uint8_t buf[MAX_PAYLOAD];
	uint16_t seqno = 0, ret = 0;

	/* send some data back through the unix socket */

	push = (struct alfred_push_data_v0 *)buf;
	push->header.type = ALFRED_PUSH_DATA;
	push->header.version = ALFRED_VERSION;
	push->tx.id = htons(id);

	while (NULL != (hashit = hash_iterate(globals->data_hash, hashit))) {
		struct dataset *dataset = hashit->bucket->data;
		struct alfred_data *data;

		if (dataset->data.header.type != requested_type)
			continue;

		data = push->data;
		memcpy(data, &dataset->data, sizeof(*data));
		data->header.length = htons(data->header.length);
		memcpy(data->data, dataset->buf, dataset->data.header.length);

		len = dataset->data.header.length + sizeof(*data);
		len += sizeof(*push) - sizeof(push->header);
		push->header.length = htons(len);
		push->tx.seqno = htons(seqno++);

		if (write(client_sock, buf, sizeof(push->header) + len) < 0) {
			ret = -1;
			break;
		}
	}

	close(client_sock);

	return ret;
}

static int unix_sock_req_data(struct globals *globals,
			      struct alfred_request_v0 *request,
			      int client_sock)
{
	int len;
	uint16_t id;
	struct transaction_head *head = NULL;

	len = ntohs(request->header.length);

	if (len != (sizeof(*request) - sizeof(request->header)))
		return -1;

	id = ntohs(request->tx_id);

	/* no server to send the request to, only give back what we have now. */
	if (!globals->best_server)
		return unix_sock_req_data_reply(globals, client_sock, id,
						request->requested_type);

	/* a master already has data to respond with */
	if (globals->opmode == OPMODE_MASTER)
		return unix_sock_req_data_reply(globals, client_sock, id,
						request->requested_type);

	head = transaction_add(globals, globals->best_server->hwaddr, id);
	if (!head)
		return -1;

	head->client_socket = client_sock;
	head->requested_type = request->requested_type;

	send_alfred_packet(globals, &globals->best_server->address,
			   request, sizeof(*request));

	return 0;
}

int unix_sock_req_data_finish(struct globals *globals,
			      struct transaction_head *head)
{
	struct alfred_status_v0 status;
	int ret = 0, send_data = 1;
	int client_sock;
	uint16_t id;
	uint8_t requested_type;

	requested_type = head->requested_type;
	id = head->id;
	client_sock = head->client_socket;
	if (head->finished != 1)
		send_data = 0;

	free(head);

	if (send_data) {
		unix_sock_req_data_reply(globals, client_sock, id,
					 requested_type);
		return 0;
	}

	status.header.type = ALFRED_STATUS_ERROR;
	status.header.version = ALFRED_VERSION;
	status.header.length = htons(sizeof(status) - sizeof(status.header));
	status.tx.id = htons(id);
	status.tx.seqno = 1;
	if (write(client_sock, &status, sizeof(status)) < 0)
		ret = -1;

	close(client_sock);
	return ret;
}

static int unix_sock_modesw(struct globals *globals,
			    struct alfred_modeswitch_v0 *modeswitch,
			    int client_sock)
{
	int len, ret = -1;

	len = ntohs(modeswitch->header.length);

	if (len < (int)(sizeof(*modeswitch) - sizeof(modeswitch->header)))
		goto err;

	switch (modeswitch->mode) {
	case ALFRED_MODESWITCH_SLAVE:
		globals->opmode = OPMODE_SLAVE;
		break;
	case ALFRED_MODESWITCH_MASTER:
		globals->opmode = OPMODE_MASTER;
		break;
	default:
		goto err;
	}

	ret = 0;
err:
	close(client_sock);
	return ret;
}

int unix_sock_read(struct globals *globals)
{
	int client_sock;
	struct sockaddr_un sun_addr;
	socklen_t sun_size = sizeof(sun_addr);
	struct alfred_tlv *packet;
	uint8_t buf[MAX_PAYLOAD];
	int length, headsize, ret = -1;

	client_sock = accept(globals->unix_sock, (struct sockaddr *)&sun_addr,
			     &sun_size);
	if (client_sock < 0) {
		fprintf(stderr, "can't accept unix connection: %s\n",
			strerror(errno));
		return -1;
	}

	/* we assume that we can instantly read here. */
	length = read(client_sock, buf, sizeof(buf));
	if (length <= 0) {
		fprintf(stderr, "read from unix socket failed: %s\n",
			strerror(errno));
		goto err;
	}

	/* drop too small packets */
	headsize = sizeof(*packet);
	if (length < headsize)
		goto err;

	packet = (struct alfred_tlv *)buf;

	if ((length - headsize) < ((int)ntohs(packet->length)))
		goto err;

	if (packet->version != ALFRED_VERSION)
		goto err;

	switch (packet->type) {
	case ALFRED_PUSH_DATA:
		ret = unix_sock_add_data(globals,
					 (struct alfred_push_data_v0 *)packet,
					 client_sock);
		break;
	case ALFRED_REQUEST:
		ret = unix_sock_req_data(globals,
					 (struct alfred_request_v0 *)packet,
					 client_sock);
		break;
	case ALFRED_MODESWITCH:
		ret = unix_sock_modesw(globals,
				       (struct alfred_modeswitch_v0 *)packet,
					client_sock);
		break;
	default:
		/* unknown packet type */
		ret = -1;
		goto err;
	}

	return ret;

err:
	close(client_sock);
	return ret;
}

int unix_sock_close(struct globals *globals)
{
	close(globals->unix_sock);
	return 0;
}
