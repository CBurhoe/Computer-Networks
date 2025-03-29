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

  /* fill in code here */
  if (ethertype(packet) == ethertype_arp) {
    struct sr_arp_hdr *packet_arp_hdr = (struct sr_arp_hdr *)(packet + sizeof(packet_eth_hdr));

    if (packet_arp_hdr->ar_op == arp_op_request) {
      //TODO: handle ARP request
    } else if (packet_arp_hdr->ar_op == arp_op_reply) {
      //TODO: handle ARP reply
    }

  } else if (ethertype(packet) == ethertype_ip) {
    struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(packet_eth_hdr));

    if (!sanity_check(packet_ip_hdr)) {
      //TODO: handle bad checksum/length (drop or send error reply?)
    }

    if (for_us(sr, packet_ip_hdr, interface)) {
      if (ip_protocol(packet) != ip_protocol_icmp) {
        //TODO: send ICMP port unreachable (type 3, code 3)
      } else if (get_icmp_type(packet) == 8) {
        //TODO: send ICMP echo reply (type 0)
      } else {
        //TODO: drop packet (just return?)
      }
    } else {
      forward_packet(sr, packet, len, interface);
    }
  }

} /* end sr_handlepacket */


/* Add any additional helper methods here & don't forget to also declare
them in sr_router.h.

If you use any of these methods in sr_arpcache.c, you must also forward declare
them in sr_arpcache.h to avoid circular dependencies. Since sr_router
already imports sr_arpcache.h, sr_arpcache cannot import sr_router.h -KM */

int sanity_check(struct sr_ip_hdr *ip_hdr) {
  //FIXME: not sure how IP checksum works, need to figure out if ip_len or ip_hl should be used in cksum
  if (cksum(ip_hdr, sizeof(struct sr_ip_hdr)) != ip_hdr->ip_sum) {
    return 0;
  }
  /*
	ip_len: packet len in bytes;
	ip_hl: header len in words (4 byte words);
	multiply ip_hl by 4 to get header length in bytes
	*/
  if (ip_hdr->ip_len < (ip_hdr->ip_hl * 4)) {
    return 0;
  }
  return 1;
}

int for_us(struct sr_instance* sr, struct sr_ip_hdr* ip_hdr, char* interface) {
  if (sr_get_interface(sr, interface) == get_interface_from_ip(sr, ip_hdr->ip_dst)) {
    return 1;
  } else { return 0; } //FIXME: Need to check other interfaces on this router as well
}

void forward_packet(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */) {
  struct sr_ethernet_hdr *packet_eth_hdr = (struct sr_ethernet_hdr *)packet;
  struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(packet_eth_hdr));
  packet_ip_hdr->ip_ttl--;
  if (packet_ip_hdr->ip_ttl <= 0) {
    //TODO: send ICMP time exceeded type 11
  }
  packet_ip_hdr->ip_sum = 0;
  packet_ip_hdr->ip_sum = cksum(packet_ip_hdr, sizeof(struct sr_ip_hdr));
  struct sr_rt *longest_match = sr_longest_prefix_match(sr, packet_ip_hdr->ip_dst);
  if (!longest_match) {
    //TODO: send ICMP destination net unreachable (type 3 code 0) and break
  }
  uint32_t next_hop_addr = longest_match->gw.s_addr;
//  if (next_hop_addr == 0) {
//    next_hop_addr = packet_ip_hdr->ip_dst; //FIXME: what does this mean???
//  }
  struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, next_hop_addr);
  if (arp_entry == NULL) {
    //TODO: queue arp request
  }

  struct sr_if *iface = sr_get_interface(sr, longest_match->interface);

  memcpy(packet_eth_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);
  memcpy(packet_eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);

  sr_send_packet(sr, packet, len, longest_match->interface);
}

int get_icmp_type(uint8_t *packet) {
  //TODO: overlay ICMP transport header after ethernet and IP headers and check the type
}

void send_icmp_packet(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */,
        unsigned int type,
        unsigned int code) {
  /*
  TODO:
  - determine length (if type 0/echo, same length, else based on headers used)
  - create new packet with malloc
  - get original IP packet's headers
  - create new packet's headers: eth, ip, icmp
  - set header fields according to help guide
  - send using sr_send_packet()
  - free new packet memory
   */
  if (type != 0) {
    //TODO: determine length
  }
  uint8_t * icmp_packet = malloc(len);
  struct sr_ethernet_hdr *new_packet_eth_hdr = (struct sr_ethernet_hdr *)icmp_packet;
  struct sr_ethernet_hdr *packet_eth_hdr = (struct sr_ethernet_hdr *)packet;
  struct sr_ip_hdr *new_packet_ip_hdr = (struct sr_ip_hdr *)(icmp_packet + sizeof(sr_ethernet_hdr_t));
  struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(sr_ethernet_hdr_t));
  struct sr_icmp_hdr *new_packet_icmp_hdr = (struct sr_icmp_hdr *)(icmp_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

  //Set the ethernet header fields
  memcpy(new_packet_eth_hdr->ether_dhost, packet_eth_hdr->ether_shost, ETHER_ADDR_LEN);
  memcpy(new_packet_eth_hdr->ether_shost, sr_get_interface(sr, interface)->addr, ETHER_ADDR_LEN);
  new_packet_eth_hdr->ether_type = ethertype_ip;

  //Set the IP header fields
  memcpy(new_packet_ip_hdr, packet_ip_hdr, sizeof(sr_ip_hdr_t));
  new_packet_ip_hdr->ip_len = len - sizeof(sr_ethernet_hdr_t);
  if (type != 0) {
    new_packet_ip_hdr->ip_off = IP_DF;
  }
  new_packet_ip_hdr->ip_ttl = 255;
  new_packet_ip_hdr->ip_p = ip_protocol_icmp;
  new_packet_ip_hdr->ip_sum = 0;
  new_packet_ip_hdr->ip_src = sr_get_interface(sr, interface)->ip; //FIXME: may have to set to another interface on the router
  new_packet_ip_hdr->ip_dst = packet_ip_hdr->ip_src;
  new_packet_ip_hdr->ip_sum = cksum(new_packet_ip_hdr, sizeof(new_packet_ip_hdr));

  //Set the ICMP header fields
  new_packet_icmp_hdr->icmp_type = type;
  new_packet_icmp_hdr->icmp_code = code;
  new_packet_icmp_hdr->icmp_sum = 0;
  if (type != 0) {
    memcpy(new_packet_icmp_hdr->data, packet_ip_hdr, sizeof(sr_ip_hdr_t)); //FIXME: may need to use ICMP_DATA_SIZE, instructions unclear
  }
  new_packet_icmp_hdr->icmp_sum = cksum(new_packet_icmp_hdr, sizeof(sr_icmp_hdr_t));

  sr_send_packet(sr, icmp_packet, len, interface); //FIXME: sending interface might be different from receiving interface

  free(icmp_packet);
}

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