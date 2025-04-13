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

	tcp_seq send_una;
	tcp_seq send_nxt;
	uint16_t send_win;
	tcp_seq rcv_nxt;
	uint16_t rcv_win;
} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);
void send_control_packet(mysocket_t sd, context_t *ctx, tcp_seq seq, tcp_seq ack, uint8_t flags);
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
	STCPHeader *syn_pack = make_packet(ctx->initial_sequence_num, 0, TH_SYN, ctx->send_win);
	ctx->send_una = ctx->initial_sequence_num;
	ctx->send_nxt = ctx->initial_sequence_num + 1;
	stcp_network_send(sd, syn_pack, sizeof(STCPHeader), NULL);
	free(syn_pack);
        
	// wait for syn ack
	stcp_wait_for_event(sd, NETWORK_DATA, NULL); 
	STCPHeader syn_ack_pack;
	stcp_network_recv(sd, &syn_ack_pack, sizeof(STCPHeader));

	if (ntohl(syn_ack_pack.th_ack) == ctx->send_una) {
		ctx->send_una += 1;
	}
	ctx->rcv_nxt = ntohl(syn_ack_pack.th_seq) + 1;
	ctx->rcv_win = ntohs(syn_ack_pack.th_win);
	
	if (syn_ack_pack.th_flags == (TH_SYN | TH_ACK)) {
		// send ack
		STCPHeader *ack_pack = make_packet(ctx->send_nxt, ctx->rcv_nxt, TH_ACK, ctx->send_win);
		stcp_network_send(sd, ack_pack, sizeof(STCPHeader), NULL);		
		ctx->connection_state = CSTATE_ESTABLISHED;
		free(ack_pack);
	}
    } else {
        // wait for syn
    	stcp_wait_for_event(sd, NETWORK_DATA, NULL); 
    	STCPHeader syn_pack;
	stcp_network_recv(sd, &syn_pack, sizeof(STCPHeader));
	
	ctx->rcv_nxt = ntohl(syn_pack.th_seq) + 1;
	ctx->rcv_win = ntohs(syn_pack.th_win);
    	if (syn_pack.th_flags == TH_SYN) {
    		// send syn ack
 	   	STCPHeader *syn_ack_pack = make_packet(ctx->send_nxt, ctx->rcv_nxt, TH_SYN | TH_ACK, ctx->send_win);
	    	stcp_network_send(sd, syn_ack_pack, sizeof(STCPHeader), NULL);
		ctx->send_una = ctx->send_nxt;
		ctx->send_nxt += 1;
		free(syn_ack_pack);
        	// wait for ack
  	  	stcp_wait_for_event(sd, NETWORK_DATA, NULL);
		STCPHeader ack_pack;
    		stcp_network_recv(sd, &ack_pack, sizeof(STCPHeader));

    		if (ack_pack.th_flags == TH_ACK) {
			ctx->send_una += 1;
			ctx->rcv_nxt = ntohl(ack_pack.th_seq) + 1;
			ctx->rcv_win = ntohs(ack_pack.th_win);
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
	ctx->initial_sequence_num = rand() / ((RAND_MAX / 256) + 1); // random number from [0,255], https://c-faq.com/lib/randrange.html
#endif
	ctx->send_una = ctx->initial_sequence_num;
	ctx->send_nxt = ctx->initial_sequence_num;
	ctx->send_win = MAX_WINDOW_SIZE;
	ctx->rcv_nxt = 1;
	ctx->rcv_nxt = MAX_WINDOW_SIZE;
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
		uint32_t empty_space = ctx->rcv_win - (ctx->send_nxt - ctx->send_una);
		size_t to_read = 0;
		if (empty_space < STCP_MSS) {
			to_read = empty_space;
		} else {
			to_read = STCP_MSS;
		}

		if (to_read) {
			char *app_data = (char *)calloc(1, to_read);
			size_t app_data_len = stcp_app_recv(sd, app_data, to_read);

			if (app_data_len > 0) {
				send_data_packet(sd, ctx, app_data, app_data_len);
				ctx->send_nxt += app_data_len;
			}
			free(app_data);
			
		}
       	}

       	if (event & NETWORK_DATA) {
            	/* received data from STCP peer */
		size_t max_packet_len = STCP_MSS + sizeof(STCPHeader);
		char *network_packet = (char *)calloc(1, max_packet_len);
		ssize_t network_packet_len = stcp_network_recv(sd, network_packet, max_packet_len); 
		dprintf(network_packet);

		STCPHeader *network_packet_header = (STCPHeader *)network_packet;
		tcp_seq peer_seq = ntohl(network_packet_header->th_seq);
		tcp_seq peer_ack = ntohl(network_packet_header->th_ack);
		uint8_t flags = network_packet_header->th_flags;
		ctx->rcv_win = ntohs(network_packet_header->th_win);
		
		char *data_for_app = network_packet + sizeof(STCPHeader);
		size_t data_len = network_packet_len - sizeof(STCPHeader);

		if (flags & TH_ACK) {
			if (peer_ack > ctx->send_una && peer_ack <= ctx->send_nxt) {
				ctx->send_una = peer_ack;
			}
			if (ctx->connection_state == CSTATE_FIN_WAIT_1) {
				ctx->connection_state = CSTATE_FIN_WAIT_2;
			}
		}

		if (flags & TH_FIN) {
			send_control_packet(sd, ctx, ctx->send_nxt, ctx->rcv_nxt, TH_ACK);
			if (ctx->connection_state == CSTATE_ESTABLISHED) {
				ctx->connection_state = CSTATE_CLOSE_WAIT;
			} else if (ctx->connection_state == CSTATE_FIN_WAIT_2) {
				ctx->connection_state = CSTATE_TIME_WAIT;
			} else if (ctx->connection_state == CSTATE_LAST_ACK) {
				ctx->done = TRUE;
			}
		}

		if (data_len > 0) {
			ctx->rcv_nxt += data_len;
		} else {
			ctx->rcv_nxt += 1;
		}

		if (data_len > 0) {
			stcp_app_send(sd, data_for_app, data_len);
			send_control_packet(sd, ctx, ctx->send_nxt, ctx->rcv_nxt, TH_ACK);
		}
		
		free(network_packet);
        }

        if (event & APP_CLOSE_REQUESTED) {
		if (ctx->connection_state == CSTATE_ESTABLISHED) {
			send_control_packet(sd, ctx, ctx->send_nxt, ctx->rcv_nxt, TH_FIN); 
			ctx->connection_state = CSTATE_FIN_WAIT_1;
			ctx->send_nxt += 1;
		} else if (ctx->connection_state == CSTATE_CLOSE_WAIT) {
			send_control_packet(sd, ctx, ctx->send_nxt, ctx->rcv_nxt, TH_FIN);
			ctx->connection_state = CSTATE_LAST_ACK;
			ctx->send_nxt += 1;
		}
	}

	if (ctx->connection_state == CSTATE_TIME_WAIT) {
		sleep(1);
		ctx->done = TRUE;
		break;
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
	pack->th_win = htons(ctx->send_una + ctx->send_win - ctx->send_nxt);
	dprintf((char *)pack);
	stcp_network_send(sd, pack, sizeof(STCPHeader), NULL);
	free(pack);
}

void send_data_packet(mysocket_t sd, context_t *ctx, void *app_data, size_t app_data_len) {
	size_t packet_len = app_data_len + sizeof(STCPHeader);
	char *packet = (char *)calloc(1, packet_len);

	STCPHeader *send_packet_header = (STCPHeader *)packet;
	send_packet_header->th_seq = htonl(ctx->send_nxt);
	send_packet_header->th_ack = htonl(ctx->rcv_nxt);
	send_packet_header->th_off = 5;
	send_packet_header->th_flags = 0;
	send_packet_header->th_win = htons(ctx->send_una + ctx->send_win - ctx->send_nxt);
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



