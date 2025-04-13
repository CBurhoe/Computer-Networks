/*
 * transport.c 
 *
 * EN.601.414/614: HW#3 (STCP)
 *
 * This file implements the STCP layer that sits between the
 * mysocket and network layers. You are required to fill in the STCP
 * functionality in this file. 
 *
 */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"


enum { 
    CSTATE_ESTABLISHED,
    CSTATE_FIN_WAIT_1,
    CSTATE_FIN_WAIT_2,
    CSTATE_TIME_WAIT,
    CSTATE_CLOSE_WAIT,
    CSTATE_LAST_ACK
};    /* obviously you should have more states */


/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;

    /* any other connection-wide global variables go here */
	tcp_seq sender_last_sequence_num;
	tcp_seq sender_next_sequence_num;
	tcp_seq receiver_last_sequence_num;
	tcp_seq receiver_last_ack;
	uint16_t receiver_win;
	tcp_seq last_delivered_seq;
	uint16_t recv_buf_size;
} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);
void send_control_packet(mysocket_t sd, context_t *ctx, tcp_seq seq, tcp_seq ack, uint8_t flags);
/*
STCPHeader *make_syn_packet(context_t *ctx);
STCPHeader *make_ack_packet(tcp_seq seq_num, tcp_seq ack_num, context_t *ctx);
STCPHeader *make_syn_ack_packet(context_t *ctx, tcp_seq syn_num);
STCPHeader *make_fin_packet(context_t *ctx);
*/
void send_data_packet(mysocket_t sd, context_t *ctx, void *app_data, size_t app_data_len);
STCPHeader *make_packet(tcp_seq seq, tcp_seq ack, uint8_t flags, uint16_t win);

#define MAX_WINDOW_SIZE 3072


/* initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
    context_t *ctx;

    ctx = (context_t *) calloc(1, sizeof(context_t));
    assert(ctx);

    generate_initial_seq_num(ctx);

    /* XXX: you should send a SYN packet here if is_active, or wait for one
     * to arrive if !is_active.  after the handshake completes, unblock the
     * application with stcp_unblock_application(sd).  you may also use
     * this to communicate an error condition back to the application, e.g.
     * if connection fails; to do so, just set errno appropriately (e.g. to
     * ECONNREFUSED, etc.) before calling the function.
     */

    if (is_active) {
        // send syn packet
	STCPHeader *syn_pack = make_packet(ctx->initial_sequence_num, 0, TH_SYN, MAX_WINDOW_SIZE);
	stcp_network_send(sd, syn_pack, sizeof(STCPHeader), NULL);
	free(syn_pack);
        // wait for syn ack
	stcp_wait_for_event(sd, NETWORK_DATA, NULL); //FIXME: probably don't want to block forever
	STCPHeader syn_ack_pack;
	stcp_network_recv(sd, &syn_ack_pack, sizeof(STCPHeader));

	//if (syn_ack_packet->th_flags != (TH_SYN | TH_ACK)) {
		//TODO: Handle missing SYN + ACK flags
	//}
	//if (syn_ack_packet->th_ack != ctx->initial_sequence_num) {
		//TODO: Handle bad ack number
	//}
	if (syn_ack_pack.th_flags == (TH_SYN | TH_ACK)) {
		// send ack
		STCPHeader *ack_pack = make_packet(ctx->sender_next_sequence_num, ntohl(syn_ack_pack.th_seq) + 1, TH_ACK, MAX_WINDOW_SIZE);
		stcp_network_send(sd, ack_pack, sizeof(STCPHeader), NULL);		
		ctx->connection_state = CSTATE_ESTABLISHED;
		free(ack_pack);
	}
    } else {
        // wait for syn
    	stcp_wait_for_event(sd, NETWORK_DATA, NULL); //FIXME: probably don't want to block forever
    	STCPHeader syn_pack;
	stcp_network_recv(sd, &syn_pack, sizeof(STCPHeader));

    	if (syn_pack.th_flags == TH_SYN) {
    		// send syn ack
 	   	STCPHeader *syn_ack_pack = make_packet(ctx->initial_sequence_num, ntohl(syn_pack.th_seq) + 1, TH_SYN | TH_ACK, MAX_WINDOW_SIZE);
	    	stcp_network_send(sd, syn_ack_pack, sizeof(STCPHeader), NULL);
		free(syn_ack_pack);
        	// wait for ack
  	  	stcp_wait_for_event(sd, NETWORK_DATA, NULL); //FIXME: probably don't want to block forever
		STCPHeader ack_pack;
    		stcp_network_recv(sd, &ack_pack, sizeof(STCPHeader));

    		if (ack_pack.th_flags == TH_ACK) {
			ctx->connection_state = CSTATE_ESTABLISHED;
    		}
    	}
    }
    //ctx->connection_state = CSTATE_ESTABLISHED;
    stcp_unblock_application(sd);

    control_loop(sd, ctx);

    /* do any cleanup here */
    free(ctx);
}


/* generate random initial sequence number for an STCP connection */
static void generate_initial_seq_num(context_t *ctx)
{
    assert(ctx);

#ifdef FIXED_INITNUM
    /* please don't change this! */
    ctx->initial_sequence_num = 1;
#else
    /* you have to fill this up */
	srand(time(0)); //Seed rand using the current time in seconds
	ctx->initial_sequence_num = rand() / ((RAND_MAX / 256) + 1); // random number from [0,255], source: https://c-faq.com/lib/randrange.html
#endif
	ctx->sender_last_sequence_num = ctx->initial_sequence_num;
	ctx->sender_next_sequence_num = ctx->initial_sequence_num + 1;
	ctx->receiver_last_ack = ctx->initial_sequence_num;
	ctx->receiver_win = MAX_WINDOW_SIZE;
	ctx->receiver_last_sequence_num = ctx->initial_sequence_num;
	ctx->last_delivered_seq = ctx->initial_sequence_num;
	ctx->recv_buf_size = MAX_WINDOW_SIZE;
}


/* control_loop() is the main STCP loop; it repeatedly waits for one of the
 * following to happen:
 *   - incoming data from the peer
 *   - new data from the application (via mywrite())
 *   - the socket to be closed (via myclose())
 *   - a timeout
 */
static void control_loop(mysocket_t sd, context_t *ctx)
{
    assert(ctx);

    while (!ctx->done)
    {
        unsigned int event;

        /* see stcp_api.h or stcp_api.c for details of this function */
        /* XXX: you will need to change some of these arguments! */
        event = stcp_wait_for_event(sd, ANY_EVENT, NULL);
       	/* check whether it was the network, app, or a close request */
        if (event & APP_DATA) {
       	    	/* the application has requested that data be sent */
            	/* see stcp_app_recv() */
		uint8_t *app_data = (uint8_t *)malloc(STCP_MSS);
		size_t app_data_len = stcp_app_recv(sd, app_data, STCP_MSS);
		//TODO: need to handle app sending more data than STCP_MSS
		if (app_data_len > 0) {
			send_data_packet(sd, ctx, app_data, app_data_len);
			ctx->sender_last_sequence_num = ctx->sender_next_sequence_num;
			ctx->sender_next_sequence_num += app_data_len;
			/*size_t in_flight = ctx->sender_next_sequence_num - ctx->receiver_last_ack;
			size_t space_left = 0;
			if (ctx->receiver_win > in_flight) {
				space_left = ctx->receiver_win - in_flight;
			}
			if (space_left > app_data_len) {
				space_left = app_data_len;
			}
			if (space_left > 0) {
				send_data_packet(sd, ctx, app_data, app_data_len);
				ctx->sender_last_sequence_num = ctx->sender_next_sequence_num;
				ctx->sender_next_sequence_num += space_left;
			}*/
		}
		free(app_data);
       	}

       	if (event & NETWORK_DATA) {
            	/* received data from STCP peer */
		size_t max_packet_len = STCP_MSS + sizeof(STCPHeader);
		char *network_packet = (char *)malloc(max_packet_len);
		ssize_t network_packet_len = stcp_network_recv(sd, network_packet, max_packet_len); //FIXME: May need to handle multiple network segments
		dprintf(network_packet);
		//stcp_app_send(sd, data_for_app, data_len);

		STCPHeader *network_packet_header = (STCPHeader *)network_packet;
		char *data_for_app = network_packet + sizeof(STCPHeader);
		size_t data_len = network_packet_len - sizeof(STCPHeader);

		tcp_seq peer_seq = ntohl(network_packet_header->th_seq);
		ctx->receiver_last_ack = ntohl(network_packet_header->th_ack);
		ctx->receiver_win = ntohs(network_packet_header->th_win);
		if (ctx->connection_state == CSTATE_FIN_WAIT_1) {
			//if ((network_packet_header->th_flags & (TH_ACK | TH_FIN)) && ctx->receiver_last_ack == ctx->sender_next_sequence_num) {
			//	send_control_packet(sd, ctx, ctx->sender_next_sequence_num, peer_seq + 1, TH_ACK);
			//	ctx->connection_state = CSTATE_TIME_WAIT;
			//	continue;
			//}
			if ((network_packet_header->th_flags & TH_ACK) && !(network_packet_header->th_flags & TH_FIN) && ctx->receiver_last_ack == ctx->sender_next_sequence_num) {
				ctx->connection_state = CSTATE_FIN_WAIT_2;
				continue;
			}
			if ((network_packet_header->th_flags & TH_ACK) && (network_packet_header->th_flags & TH_FIN) && ctx->receiver_last_ack == ctx->sender_next_sequence_num) {
				send_control_packet(sd, ctx, ctx->sender_next_sequence_num, peer_seq + 1, TH_ACK);
				ctx->connection_state = CSTATE_TIME_WAIT;
				continue;
			}

		}
		if (ctx->connection_state == CSTATE_FIN_WAIT_2) {
			if (network_packet_header->th_flags & TH_FIN) {
				ctx->receiver_last_sequence_num = peer_seq;
				send_control_packet(sd, ctx, ctx->sender_next_sequence_num, peer_seq + 1, TH_ACK);
				ctx->connection_state = CSTATE_TIME_WAIT;
			}
			continue;
		}
		if (network_packet_header->th_flags & TH_FIN) {
			ctx->receiver_last_sequence_num = peer_seq;
			send_control_packet(sd, ctx, ctx->sender_next_sequence_num, peer_seq + 1, TH_ACK);
			if (ctx->connection_state == CSTATE_ESTABLISHED) {
				ctx->connection_state = CSTATE_CLOSE_WAIT;
			} else if (ctx->connection_state == CSTATE_LAST_ACK) {
				ctx->done = TRUE;
			}
		}
		if (ctx->connection_state == CSTATE_LAST_ACK) {
			if ((network_packet_header->th_flags & TH_ACK) && ctx->receiver_last_ack == ctx->sender_next_sequence_num) {
				ctx->done = TRUE;
			}
		}

		if (data_len > 0) {
			stcp_app_send(sd, data_for_app, data_len);
			ctx->receiver_last_sequence_num = peer_seq + data_len;
			ctx->last_delivered_seq = ctx->receiver_last_sequence_num;
			send_control_packet(sd, ctx, ctx->sender_next_sequence_num, ctx->receiver_last_sequence_num, TH_ACK);
		}
		
		free(network_packet);
        }

        if (event & APP_CLOSE_REQUESTED) {
		if (ctx->connection_state == CSTATE_ESTABLISHED) {
			send_control_packet(sd, ctx, ctx->sender_next_sequence_num, ctx->receiver_last_sequence_num, TH_FIN); //FIXME
			ctx->connection_state = CSTATE_FIN_WAIT_1;
			ctx->sender_next_sequence_num += 1;
		} else if (ctx->connection_state == CSTATE_CLOSE_WAIT) {
			send_control_packet(sd, ctx, ctx->sender_next_sequence_num, ctx->receiver_last_sequence_num, TH_FIN); //FIXME
			ctx->connection_state = CSTATE_LAST_ACK;
			ctx->sender_next_sequence_num += 1;
		}
	}

	if (ctx->connection_state == CSTATE_TIME_WAIT) {
		//sleep(1); FIXME: wait for 2*MSL
		ctx->done = TRUE;
	}
    }
}

STCPHeader *make_packet(tcp_seq seq, tcp_seq ack, uint8_t flags, uint16_t win) {
	STCPHeader *packet = (STCPHeader *)calloc(1, sizeof(STCPHeader));
	packet->th_seq = htonl(seq);
	packet->th_ack = htonl(ack);
	packet->th_off = 5;
	packet->th_flags = flags;
	packet->th_win = htons(win);
	return packet;
}
void send_control_packet(mysocket_t sd, context_t *ctx, tcp_seq seq, tcp_seq ack, uint8_t flags) {
	STCPHeader *pack = (STCPHeader *)calloc(1, sizeof(STCPHeader));
	pack->th_seq = htonl(seq);
	pack->th_ack = htonl(ack);
	pack->th_off = 5;
	pack->th_flags = flags;
	pack->th_win = htons(ctx->recv_buf_size - (ctx->receiver_last_sequence_num - ctx->last_delivered_seq));
	dprintf((char *)pack);
	stcp_network_send(sd, pack, sizeof(STCPHeader), NULL);
	free(pack);
}
/*STCPHeader *make_fin_packet(context_t *ctx) {
	STCPHeader *fin_pack = (STCPHeader *)calloc(1, sizeof(STCPHeader));
	fin_pack->th_seq = htonl(ctx->sender_next_sequence_num);
	fin_pack->th_ack = htonl(ctx->receiver_last_sequence_num + 1);
	fin_pack->th_off = 5;
	fin_pack->th_flags = TH_FIN;
	fin_pack->th_win = htons(3702); //FIXME: figure out actual window size
	return fin_pack;
}

STCPHeader  *make_syn_packet(context_t *ctx) {
	STCPHeader *syn_pack = (STCPHeader *)malloc(sizeof(STCPHeader));
	memset(syn_pack, 0, sizeof(STCPHeader));
	syn_pack->th_seq = htonl(ctx->initial_sequence_num);
	syn_pack->th_ack = 0;
	syn_pack->th_off = 5;
	syn_pack->th_flags = TH_SYN;
	syn_pack->th_win = htons(3072);
	ctx->sender_last_sequence_num = ctx->initial_sequence_num;
	return syn_pack;
}

STCPHeader *make_ack_packet(tcp_seq seq_num, tcp_seq ack_num, context_t *ctx) {
	//TODO: create ack packet using seq and ack numbers from SYN ACK packet
	STCPHeader *ack_pack = (STCPHeader *)malloc(sizeof(STCPHeader));
	memset(ack_pack, 0, sizeof(STCPHeader));
	ack_pack->th_seq = htonl(ctx->sender_last_sequence_num + 1); //FIXME: Possibly initial_sequence_num + 1 but don't know
	ack_pack->th_ack = htonl(seq_num + 1); //FIXME: I think ack num = last seq but I may be wrong
	ack_pack->th_off = 5;
	ack_pack->th_flags = TH_ACK;
	ack_pack->th_win = 0; //FIXME: no idea what the new window val is
	return ack_pack;
}

STCPHeader *make_syn_ack_packet(context_t *ctx, tcp_seq syn_num) {
	STCPHeader *syn_ack_pack = (STCPHeader *)malloc(sizeof(STCPHeader));
	memset(syn_ack_pack, 0, sizeof(STCPHeader));
	syn_ack_pack->th_seq = htonl(ctx->initial_sequence_num);
	syn_ack_pack->th_ack = htonl(syn_num + 1);
	syn_ack_pack->th_off = 5;
	syn_ack_pack->th_flags = TH_SYN | TH_ACK;
	syn_ack_pack->th_win = htons(3072); //FIXME: don't know if it's still the same
	return syn_ack_pack;
}*/

void send_data_packet(mysocket_t sd, context_t *ctx, void *app_data, size_t app_data_len) {
	size_t packet_len = app_data_len + sizeof(STCPHeader);
	char *packet = (char *)calloc(1, packet_len);

	STCPHeader *send_packet_header = (STCPHeader *)packet;
	// Set header fields
	send_packet_header->th_seq = htonl(ctx->sender_next_sequence_num);
	send_packet_header->th_ack = htonl(ctx->receiver_last_sequence_num);
	send_packet_header->th_off = 5;
	send_packet_header->th_flags = 0;
	send_packet_header->th_win = htons(ctx->recv_buf_size - (ctx->receiver_last_sequence_num - ctx->last_delivered_seq)); //FIXME: need to set window
	// Copy data
	memcpy(packet + sizeof(STCPHeader), app_data, app_data_len);
	dprintf(packet);
	stcp_network_send(sd, packet, packet_len, NULL);
	free(packet);
}


/**********************************************************************/
/* our_dprintf
 *
 * Send a formatted message to stdout.
 * 
 * format               A printf-style format string.
 *
 * This function is equivalent to a printf, but may be
 * changed to log errors to a file if desired.
 *
 * Calls to this function are generated by the dprintf amd
 * dperror macros in transport.h
 */
void our_dprintf(const char *format,...)
{
    va_list argptr;
    char buffer[1024];

    assert(format);
    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    fputs(buffer, stdout);
    fflush(stdout);
}



