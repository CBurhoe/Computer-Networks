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
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"


enum { 
    CSTATE_ESTABLISHED

};    /* obviously you should have more states */


/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;

    /* any other connection-wide global variables go here */
} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);
STCPHeader *make_syn_packet(context_t *ctx);
STCPHeader *make_ack_packet(tcp_seq seq_num, tcp_seq ack_num);
STCPHeader *make_syn_ack_packet(context_t *ctx, tcp_seq syn_num);


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
		STCPHeader *syn_pack = make_syn_packet(ctx);
		size_t syn_pack_len = sizeof(STCPHeader);
		ssize_t bytes_sent = stcp_network_send(sd, syn_pack, syn_pack_len, NULL);

        // wait for syn ack
		unsigned int event_mask = stcp_wait_for_event(sd, NETWORK_DATA, NULL); //FIXME: probably don't want to block forever
		uint8_t *recv_packet = (uint8_t *)malloc(sizeof(STCPHeader));
		ssize_t recv_packet_bytes = stcp_network_recv(sd, recv_packet, sizeof(STCPHeader));

		STCPHeader *syn_ack_packet = (STCPHeader *)recv_packet;

		if (syn_ack_packet->th_flags != (TH_SYN | TH_ACK)) {
			//TODO: Handle missing SYN + ACK flags
		}
		if (syn_ack_packet->th_ack != ctx->initial_sequence_num) {
			//TODO: Handle bad ack number
		}
		tcp_seq ack_num = syn_ack_packet->th_ack;
		tcp_seq receiver_seq_number = syn_ack_packet->th_seq;
        
		// send ack
		STCPHeader *ack_pack = make_ack_packet(receiver_seq_number, ack_num);
		size_t ack_pack_len = sizeof(STCPHeader);
		ssize_t ack_bytes_sent = stcp_network_send(sd, ack_pack, ack_pack_len, NULL);

    } else {
        // wait for syn
    	unsigned int event_mask = stcp_wait_for_event(sd, NETWORK_DATA, NULL); //FIXME: probably don't want to block forever
    	uint8_t *recv_packet = (uint8_t *)malloc(sizeof(STCPHeader));
		ssize_t recv_packet_bytes = stcp_network_recv(sd, recv_packet, sizeof(STCPHeader));

    	STCPHeader *syn_packet = (STCPHeader *)recv_packet;

    	if (syn_packet->th_flags != TH_SYN) {
    		//TODO: Handle missing SYN flag
    	}

    	tcp_seq receiver_ack_num = syn_packet->th_ack;
    	tcp_seq receiver_seq_num = syn_packet->th_seq;


        // send syn ack
    	STCPHeader *syn_ack_pack = make_syn_ack_packet(ctx, receiver_seq_num);
    	size_t syn_ack_pack_len = sizeof(STCPHeader);

    	ssize_t syn_ack_bytes_sent = stcp_network_send(sd, syn_ack_pack, syn_ack_pack_len, NULL);

        // wait for ack
    	event_mask = stcp_wait_for_event(sd, NETWORK_DATA, NULL); //FIXME: probably don't want to block forever
    	recv_packet_bytes = stcp_network_recv(sd, recv_packet, sizeof(STCPHeader));
    	STCPHeader *ack_packet = (STCPHeader *)recv_packet;

    	if (ack_packet->th_flags != TH_ACK) {
    		//TODO: Handle missing ACK flag
    	}
    	receiver_ack_num = ack_packet->th_ack;
    	receiver_seq_num = ack_packet->th_seq;

    }
    ctx->connection_state = CSTATE_ESTABLISHED;
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
        if (event & APP_DATA)
        {
            /* the application has requested that data be sent */
            /* see stcp_app_recv() */
        }

        if (event & NETWORK_DATA) {
            /* received data from STCP peer */
        }

        if (event & APP_CLOSE_REQUESTED) {

        }

        /* etc. */
    }
}

STCPHeader  *make_syn_packet(context_t *ctx) {
	STCPHeader *syn_pack = (STCPHeader *)malloc(sizeof(STCPHeader));
	memset(syn_pack, 0, sizeof(STCPHeader));
	syn_pack->th_seq = htonl(ctx->initial_sequence_num);
	syn_pack->th_ack = 0;
	syn_pack->th_off = 5;
	syn_pack->th_flags = TH_SYN;
	syn_pack->th_win = htons(3072);
	return syn_pack;
}

STCPHeader *make_ack_packet(tcp_seq seq_num, tcp_seq ack_num, context_t *ctx) {
	//TODO: create ack packet using seq and ack numbers from SYN ACK packet
	STCPHeader *ack_pack = (STCPHeader *)malloc(sizeof(STCPHeader));
	memset(ack_pack, 0, sizeof(STCPHeader));
	ack_pack->th_seq = htonl(ctx->initial_sequence_num + 1); //FIXME: Possibly initial_sequence_num + 1 but don't know
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



