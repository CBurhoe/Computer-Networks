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

  print_hdrs(packet, len);

  struct sr_ethernet_hdr *packet_eth_hdr = (struct sr_ethernet_hdr *)packet;

  /* fill in code here */
  if (ethertype(packet) == ethertype_arp) {
    struct sr_arp_hdr *packet_arp_hdr = (struct sr_arp_hdr *)(packet + sizeof(sr_ethernet_hdr_t));

    arp_hdr_to_host(packet_arp_hdr);

    if (packet_arp_hdr->ar_op == arp_op_request) {
      if (for_us(sr, packet_arp_hdr->ar_tip, interface)) {
        printf("IT'S FOR US\n");
        uint8_t *arp_reply = (uint8_t *)malloc(len);
        struct sr_ethernet_hdr *arp_reply_eth_hdr = (struct sr_ethernet_hdr *)arp_reply;
        struct sr_arp_hdr *arp_reply_arp_hdr = (struct sr_arp_hdr *)(arp_reply + sizeof(sr_ethernet_hdr_t));
        //Set the ethernet header fields
        memcpy(arp_reply_eth_hdr->ether_dhost, packet_eth_hdr->ether_shost, ETHER_ADDR_LEN);
        memcpy(arp_reply_eth_hdr->ether_shost, sr_get_interface(sr, interface)->addr, ETHER_ADDR_LEN);
        arp_reply_eth_hdr->ether_type = htons(ethertype_arp);

        //Set the ARP header fields
        arp_reply_arp_hdr->ar_hrd = htons(arp_hrd_ethernet);
        arp_reply_arp_hdr->ar_pro = htons(ethertype_ip);
        arp_reply_arp_hdr->ar_hln = 6;
        arp_reply_arp_hdr->ar_pln = 4;
        arp_reply_arp_hdr->ar_op = htons(arp_op_reply);
        memcpy(arp_reply_arp_hdr->ar_sha, sr_get_interface(sr, interface)->addr, ETHER_ADDR_LEN);
        arp_reply_arp_hdr->ar_sip = sr_get_interface(sr, interface)->ip;
        memcpy(arp_reply_arp_hdr->ar_tha, packet_eth_hdr->ether_shost, ETHER_ADDR_LEN);
        arp_reply_arp_hdr->ar_tip = htonl(packet_arp_hdr->ar_sip);


        print_hdrs(arp_reply, len);
        sr_send_packet(sr, arp_reply, len, interface);

        free(arp_reply);
        return;
      } else {
        return;
      }
    } else if (packet_arp_hdr->ar_op == arp_op_reply) {
      if (for_us(sr, packet_arp_hdr->ar_tip, interface)) {
        printf("IT'S FOR US\n");
        struct sr_arpreq *request_queue = sr_arpcache_insert(&sr->cache, packet_arp_hdr->ar_sha, htonl(packet_arp_hdr->ar_sip));
        if (request_queue) {
          printf("There were others waiting\n");
          struct sr_packet *queued_packet = request_queue->packets;
          while (queued_packet) {
            struct sr_ethernet_hdr *queued_packet_eth_hdr = (struct sr_ethernet_hdr *)queued_packet->buf;
            memcpy(queued_packet_eth_hdr->ether_dhost, packet_arp_hdr->ar_sha, ETHER_ADDR_LEN);
            queued_packet_eth_hdr->ether_type = htons(queued_packet_eth_hdr->ether_type);
            print_hdrs(queued_packet->buf, queued_packet->len);
            sr_send_packet(sr, queued_packet->buf, queued_packet->len, interface);
            queued_packet = queued_packet->next;
          }
          sr_arpreq_destroy(&sr->cache, request_queue);
        }
      }
      return;
    }

  } else if (ethertype(packet) == ethertype_ip) {
    struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(sr_ethernet_hdr_t));


    if (!sanity_check(packet_ip_hdr)) { //want to check checksum while packet is still in network byte order
      printf("IP header check failed\n");
      return; //Discard the packet
    }
    printf("Seems to be a good IP packet we have\n");
    ip_hdr_to_host(packet_ip_hdr); //convert to host byte order AFTER checking checksum

    if (for_us(sr, packet_ip_hdr->ip_dst, interface)) {
      printf("THIS ONE IS FOR US\n");
      if (ip_protocol(packet) != ip_protocol_icmp) {
        send_icmp_packet(sr, packet, len, interface, 3, 3);
      } else if (get_icmp_type(packet) == 8) {
        send_icmp_packet(sr, packet, len, interface, 0, 0);
      } else {
        return; //ICMP packet but not ECHO, drop packet
      }
    } else {
      printf("not for us... forwarding\n");
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
  uint16_t their_sum = ip_hdr->ip_sum;
  ip_hdr->ip_sum = 0;
  if (cksum(ip_hdr, ip_hdr->ip_hl * 4) != their_sum) {
    return 0;
  }
  ip_hdr->ip_sum = their_sum;
  /*
	ip_len: packet len in bytes;
	ip_hl: header len in words (4 byte words);
	multiply ip_hl by 4 to get header length in bytes
	*/
  if (ntohs(ip_hdr->ip_len) < (ip_hdr->ip_hl * 4)) {
    return 0;
  }
  return 1;
}

int for_us(struct sr_instance* sr, uint32_t ip_addr, char* interface) {
  struct sr_if *iface = sr->if_list;

  while(iface) {
    printf("Checking:\n");
    print_addr_ip_int(ntohl(iface->ip));
    if (ntohl(iface->ip) == ip_addr) {
      return 1;
    }
    iface = iface->next;
  }
  return 0;
}

void forward_packet(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */) {
  uint8_t *fwd_packet = malloc(len);
  memcpy(fwd_packet, packet, len);

  struct sr_ethernet_hdr *packet_eth_hdr = (struct sr_ethernet_hdr *)fwd_packet;
  struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(fwd_packet + sizeof(sr_ethernet_hdr_t));
  packet_ip_hdr->ip_ttl--;
  if (packet_ip_hdr->ip_ttl <= 0) {
    send_icmp_packet(sr, fwd_packet, len, interface, 11, 0);
    free(fwd_packet);
    return;
  }
  packet_ip_hdr->ip_sum = 0;
  ip_hdr_to_network(packet_ip_hdr); //Not my favorite solution, but I'll fix it later
  packet_ip_hdr->ip_sum = cksum(packet_ip_hdr, packet_ip_hdr->ip_hl * 4);
  ip_hdr_to_host(packet_ip_hdr); //Really hate this...

  struct sr_rt *longest_match = sr_longest_prefix_match(sr, packet_ip_hdr->ip_dst);
  if (!longest_match) {
    send_icmp_packet(sr, fwd_packet, len, interface, 3, 0);
    free(fwd_packet);
    return;
  }
  uint32_t next_hop_addr = longest_match->gw.s_addr;
//  if (next_hop_addr == 0) {
//    next_hop_addr = packet_ip_hdr->ip_dst; //FIXME: what does this mean???
//  }
  printf("Is there an arp entry?\n");
  struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, next_hop_addr);
  if (!arp_entry) {
    printf("No arp entry, creating request...\n");
    ip_hdr_to_network(packet_ip_hdr);
    packet_eth_hdr->ether_type = htons(packet_eth_hdr->ether_type);
    struct sr_arpreq *request = sr_arpcache_queuereq(&sr->cache, next_hop_addr, fwd_packet, len, longest_match->interface); //FIXME: check ip address argument; also do we need to do something with the pointer returned here?
    send_arpreq(sr, len, longest_match->interface, request);
    return;
  }

  printf("There is an arp entry\n");
  struct sr_if *iface = sr_get_interface(sr, longest_match->interface);

  memcpy(packet_eth_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);
  memcpy(packet_eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);

  packet_eth_hdr->ether_type = htons(packet_eth_hdr->ether_type);
  ip_hdr_to_network(packet_ip_hdr);

  printf("Sending the packet to be forwarded...\n");
  print_hdrs(fwd_packet, len);
  sr_send_packet(sr, fwd_packet, len, longest_match->interface);
  free(fwd_packet);
  free(arp_entry);
}

int get_icmp_type(uint8_t *packet) {
  struct sr_icmp_hdr *packet_icmp_hdr = (struct sr_icmp_hdr *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  return packet_icmp_hdr->icmp_type;
}

void send_icmp_packet(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */,
        unsigned int type,
        unsigned int code) {
  if (type != 0) {
    len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t);
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
  new_packet_eth_hdr->ether_type = htons(ethertype_ip);

  //Set the IP header fields
  memcpy(new_packet_ip_hdr, packet_ip_hdr, sizeof(sr_ip_hdr_t));
  new_packet_ip_hdr->ip_len = len - sizeof(sr_ethernet_hdr_t);
  if (type != 0) {
    new_packet_ip_hdr->ip_off = IP_DF;
  }
  new_packet_ip_hdr->ip_ttl = 255;
  new_packet_ip_hdr->ip_p = ip_protocol_icmp;
  new_packet_ip_hdr->ip_sum = 0;
  new_packet_ip_hdr->ip_src = ntohl(sr_get_interface(sr, interface)->ip);
  new_packet_ip_hdr->ip_dst = packet_ip_hdr->ip_src;
  ip_hdr_to_network(new_packet_ip_hdr);
  new_packet_ip_hdr->ip_sum = cksum(new_packet_ip_hdr, sizeof(new_packet_ip_hdr));

  //Set the ICMP header fields
  new_packet_icmp_hdr->icmp_type = type;
  new_packet_icmp_hdr->icmp_code = code;
  new_packet_icmp_hdr->icmp_sum = 0;
  if (type != 0) {
    memcpy(new_packet_icmp_hdr->data, new_packet_ip_hdr, sizeof(sr_ip_hdr_t)); //FIXME: may need to use ICMP_DATA_SIZE, instructions unclear
  }

  icmp_hdr_to_network(new_packet_icmp_hdr);
  new_packet_icmp_hdr->icmp_sum = cksum(new_packet_icmp_hdr, sizeof(sr_icmp_hdr_t));

  print_hdrs(icmp_packet, len);
  sr_send_packet(sr, icmp_packet, len, interface); //FIXME: sending interface might be different from receiving interface

  free(icmp_packet);
}

void send_arpreq(struct sr_instance* sr,
        unsigned int len,
        char* interface/* lent */,
        struct sr_arpreq *request) {
  printf("Hi, welcome to the send_arpreq() function\n");
  uint8_t *arp_request = (uint8_t *)malloc(sizeof(sr_arp_hdr_t) + sizeof(sr_ethernet_hdr_t));
  struct sr_ethernet_hdr *arp_reply_eth_hdr = (struct sr_ethernet_hdr *)arp_request;
  struct sr_arp_hdr *arp_reply_arp_hdr = (struct sr_arp_hdr *)(arp_request + sizeof(sr_ethernet_hdr_t));

  struct sr_if *iface = sr_get_interface(sr, request->packets->iface);
  //Set the ethernet header fields
  memset(arp_reply_eth_hdr->ether_dhost, 0xFF, ETHER_ADDR_LEN);
  memcpy(arp_reply_eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
  arp_reply_eth_hdr->ether_type = htons(ethertype_arp);

  //Set the ARP header fields
  arp_reply_arp_hdr->ar_hrd = htons(arp_hrd_ethernet);
  arp_reply_arp_hdr->ar_pro = htons(ethertype_ip);
  arp_reply_arp_hdr->ar_hln = 6;
  arp_reply_arp_hdr->ar_pln = 4;
  arp_reply_arp_hdr->ar_op = htons(arp_op_request);
  memcpy(arp_reply_arp_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
  arp_reply_arp_hdr->ar_sip = iface->ip;
  memset(arp_reply_arp_hdr->ar_tha, 0xFF, ETHER_ADDR_LEN);
  arp_reply_arp_hdr->ar_tip = request->ip;

//  arp_hdr_to_network(arp_reply_arp_hdr);
  print_hdrs(arp_request, sizeof(sr_arp_hdr_t) + sizeof(sr_ethernet_hdr_t));
  sr_send_packet(sr, arp_request, sizeof(sr_arp_hdr_t) + sizeof(sr_ethernet_hdr_t), iface->name);

  free(arp_request);
  return;
}

struct sr_rt *sr_longest_prefix_match(struct sr_instance *sr, uint32_t dest_ip) { //FIXME: need to look at host vs net order here
  struct sr_rt* rt_walker = sr->routing_table;
  struct sr_rt* longest_prefix_match = NULL;
  uint32_t longest_mask = 0;

  while(rt_walker) {
    if ((dest_ip & ntohl(rt_walker->mask.s_addr)) == ntohl(rt_walker->dest.s_addr)) {
      if (ntohl(rt_walker->mask.s_addr) > ntohl(longest_mask)) {
        longest_mask = rt_walker->mask.s_addr;
        longest_prefix_match = rt_walker;
      }
    }
    rt_walker = rt_walker->next;
  }
  return longest_prefix_match;

}

void ip_hdr_to_host(struct sr_ip_hdr *ip_hdr) {
  uint16_t tmp_s = 0;
  tmp_s = ntohs(ip_hdr->ip_len);
  ip_hdr->ip_len = tmp_s;
  tmp_s = ntohs(ip_hdr->ip_id);
  ip_hdr->ip_id = tmp_s;
  tmp_s = ntohs(ip_hdr->ip_off);
  ip_hdr->ip_off = tmp_s;
  tmp_s = ntohs(ip_hdr->ip_sum);
  ip_hdr->ip_sum = tmp_s;
  uint32_t tmp_l = 0;
  tmp_l = ntohl(ip_hdr->ip_src);
  ip_hdr->ip_src = tmp_l;
  tmp_l = ntohl(ip_hdr->ip_dst);
  ip_hdr->ip_dst = tmp_l;
}

void ip_hdr_to_network(struct sr_ip_hdr *ip_hdr) {
  uint16_t tmp_s = 0;
  tmp_s = htons(ip_hdr->ip_len);
  ip_hdr->ip_len = tmp_s;
  tmp_s = htons(ip_hdr->ip_id);
  ip_hdr->ip_id = tmp_s;
  tmp_s = htons(ip_hdr->ip_off);
  ip_hdr->ip_off = tmp_s;
  tmp_s = htons(ip_hdr->ip_sum);
  ip_hdr->ip_sum = tmp_s;
  uint32_t tmp_l = 0;
  tmp_l = htonl(ip_hdr->ip_src);
  ip_hdr->ip_src = tmp_l;
  tmp_l = htonl(ip_hdr->ip_dst);
  ip_hdr->ip_dst = tmp_l;
}

void arp_hdr_to_host(struct sr_arp_hdr *arp_hdr) {
  unsigned short tmp_s = 0;
  tmp_s = ntohs(arp_hdr->ar_hrd);
  arp_hdr->ar_hrd = tmp_s;
  tmp_s = ntohs(arp_hdr->ar_pro);
  arp_hdr->ar_pro = tmp_s;
  tmp_s = ntohs(arp_hdr->ar_op);
  arp_hdr->ar_op = tmp_s;

  unsigned long tmp_l = 0;
  tmp_l = ntohl(arp_hdr->ar_sip);
  arp_hdr->ar_sip = tmp_l;
  tmp_l = ntohl(arp_hdr->ar_tip);
  arp_hdr->ar_tip = tmp_l;
}

void arp_hdr_to_network(struct sr_arp_hdr *arp_hdr) {
  unsigned short tmp_s = 0;
  tmp_s = htons(arp_hdr->ar_hrd);
  arp_hdr->ar_hrd = tmp_s;
  tmp_s = htons(arp_hdr->ar_pro);
  arp_hdr->ar_pro = tmp_s;
  tmp_s = htons(arp_hdr->ar_op);
  arp_hdr->ar_op = tmp_s;

  unsigned long tmp_l = 0;
  tmp_l = htonl(arp_hdr->ar_sip);
  arp_hdr->ar_sip = tmp_l;
  tmp_l = htonl(arp_hdr->ar_tip);
  arp_hdr->ar_tip = tmp_l;
}

void icmp_hdr_to_host(struct sr_icmp_hdr *icmp_hdr) {
  uint16_t tmp_s = 0;
  tmp_s = ntohs(icmp_hdr->icmp_sum);
  icmp_hdr->icmp_sum = tmp_s;
}

void icmp_hdr_to_network(struct sr_icmp_hdr *icmp_hdr) {
  uint16_t tmp_s = 0;
  tmp_s = htons(icmp_hdr->icmp_sum);
  icmp_hdr->icmp_sum = tmp_s;
}