/**********************************************************************
 * file:  sr_router.c
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  struct sr_ethernet_hdr *packet_eth_hdr = (struct sr_ethernet_hdr *)packet;
  //memcpy(&packet_eth_hdr, packet, sizeof(packet_eth_hdr)); //FIXME: overlay, don't copy

  /* fill in code here */
  if (ethertype(packet) == ethertype_arp) {
    //TODO: handle arp packet
    struct sr_arp_hdr packet_arp_hdr;
    memcpy(&packet_arp_hdr, packet + sizeof(packet_eth_hdr), sizeof(packet_arp_hdr));

    if (packet_arp_hdr.ar_op == arp_op_request) {
      //TODO: handle ARP request
    } else if (packet_arp_hdr.ar_op == arp_op_reply) {
      //TODO: handle ARP reply
    }

  } else if (ethertype(packet) == ethertype_ip) {
    struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(packet_eth_hdr));
//    memcpy(&packet_ip_hdr, packet + sizeof(packet_eth_hdr), sizeof(packet_ip_hdr));

    //FIXME: not sure how IP checksum works, need to figure out if ip_len or ip_hl should be used in cksum
    if (cksum(&packet_ip_hdr, sizeof(packet_ip_hdr)) != packet_ip_hdr.ip_sum) {
      //TODO: handle bad checksum
    }

	/*
	ip_len: packet len in bytes;
	ip_hl: header len in words (4 byte words);
	multiply ip_hl by 4 to get header length in bytes
	*/
    if (packet_ip_hdr.ip_len < (packet_ip_hdr.ip_hl * 4)) {
      //TODO: handle bad length
    }

    //FIXME: may need to check other interfaces on this router
    if (sr_get_interface(sr, interface) == get_interface_from_ip(sr, packet_ip_hdr.ip_dst)) {
      //TODO: handle packet destined for this interface
    } else {
      packet_ip_hdr.ip_ttl--;
      if (packet_ip_hdr.ip_ttl <= 0) {
        //TODO: send ICMP time exceeded type 11
      }
      packet_ip_hdr.ip_sum = cksum(&packet_ip_hdr, sizeof(packet_ip_hdr));
      //TODO: forward packet to dst
    }
  }

} /* end sr_handlepacket */


/* Add any additional helper methods here & don't forget to also declare
them in sr_router.h.

If you use any of these methods in sr_arpcache.c, you must also forward declare
them in sr_arpcache.h to avoid circular dependencies. Since sr_router
already imports sr_arpcache.h, sr_arpcache cannot import sr_router.h -KM */

struct sr_rt *sr_longest_prefix_match(struct sr_instance *sr, uint32_t dest_ip) {
  struct sr_rt* rt_walker = sr->routing_table;
  struct sr_rt* longest_prefix_match = NULL;
  uint32_t longest_mask = 0;

  while(rt_walker) {
    if ((dest_ip & rt_walker->mask.s_addr) == rt_walker->dest.s_addr) {
      if (ntohl(rt_walker->mask.s_addr) > ntohl(longest_mask)) {
        longest_mask = rt_walker->mask.s_addr;
        longest_prefix_match = rt_walker;
      }
    }
    rt_walker = rt_walker->next;
  }
  return longest_prefix_match;

}