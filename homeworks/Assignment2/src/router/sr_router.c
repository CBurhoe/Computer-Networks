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
    struct sr_arp_hdr *packet_arp_hdr = (struct sr_arp_hdr *)(packet + sizeof(sr_ethernet_hdr_t));

    arp_hdr_to_host(packet_arp_hdr);

    if (packet_arp_hdr->ar_op == arp_op_request) {
      if (for_us(sr, packet_arp_hdr->ar_tip, interface)) {
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


        sr_send_packet(sr, arp_reply, len, interface);

        free(arp_reply);
        return;
      } else {
        return;
      }
    } else if (packet_arp_hdr->ar_op == arp_op_reply) {
      if (for_us(sr, packet_arp_hdr->ar_tip, interface)) {
        struct sr_arpreq *request_queue = sr_arpcache_insert(&sr->cache, packet_arp_hdr->ar_sha, htonl(packet_arp_hdr->ar_sip));
        if (request_queue) {
          struct sr_packet *queued_packet = request_queue->packets;
          while (queued_packet) {
            struct sr_ethernet_hdr *queued_packet_eth_hdr = (struct sr_ethernet_hdr *)queued_packet->buf;
            memcpy(queued_packet_eth_hdr->ether_shost, sr_get_interface(sr, queued_packet->iface)->addr, ETHER_ADDR_LEN);
            memcpy(queued_packet_eth_hdr->ether_dhost, packet_arp_hdr->ar_sha, ETHER_ADDR_LEN);
            queued_packet_eth_hdr->ether_type = htons(queued_packet_eth_hdr->ether_type);
            sr_send_packet(sr, queued_packet->buf, queued_packet->len, queued_packet->iface);
            queued_packet = queued_packet->next;
          }
          sr_arpreq_destroy(&sr->cache, request_queue);
        }
      }
      return;
    }

  } else if (ethertype(packet) == ethertype_ip) {
    struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(sr_ethernet_hdr_t));


    if (!sanity_check(packet_ip_hdr)) {
      return;
    }
    ip_hdr_to_host(packet_ip_hdr);

    if (for_us(sr, packet_ip_hdr->ip_dst, interface)) {
      if (packet_ip_hdr->ip_p != ip_protocol_icmp) {
        send_icmp_packet(sr, packet, len, interface, 3, 3);
      } else if (get_icmp_type(packet) == 8) {
        send_icmp_packet(sr, packet, len, interface, 0, 0);
      } else {
        return;
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
  uint16_t their_sum = ip_hdr->ip_sum;
  ip_hdr->ip_sum = 0;
  if (cksum(ip_hdr, ip_hdr->ip_hl * 4) != their_sum) {
    return 0;
  }
  ip_hdr->ip_sum = their_sum;

  if (ntohs(ip_hdr->ip_len) < (ip_hdr->ip_hl * 4)) {
    return 0;
  }
  return 1;
}

int for_us(struct sr_instance* sr, uint32_t ip_addr, char* interface) {
  struct sr_if *iface = sr->if_list;

  while(iface) {
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
  ip_hdr_to_network(packet_ip_hdr);
  packet_ip_hdr->ip_sum = cksum(packet_ip_hdr, packet_ip_hdr->ip_hl * 4);
  ip_hdr_to_host(packet_ip_hdr);

  struct sr_rt *longest_match = sr_longest_prefix_match(sr, packet_ip_hdr->ip_dst);
  if (!longest_match) {
    send_icmp_packet(sr, fwd_packet, len, interface, 3, 0);
    free(fwd_packet);
    return;
  }
  uint32_t next_hop_addr = longest_match->gw.s_addr;

  struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, next_hop_addr);
  if (!arp_entry) {
    ip_hdr_to_network(packet_ip_hdr);
    packet_eth_hdr->ether_type = htons(packet_eth_hdr->ether_type);
    struct sr_arpreq *request = sr_arpcache_queuereq(&sr->cache, next_hop_addr, fwd_packet, len, longest_match->interface);
    send_arpreq(sr, len, longest_match->interface, request);
    return;
  }

  struct sr_if *iface = sr_get_interface(sr, longest_match->interface);

  memcpy(packet_eth_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);
  memcpy(packet_eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);

  packet_eth_hdr->ether_type = packet_eth_hdr->ether_type;
  ip_hdr_to_network(packet_ip_hdr);

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
  uint8_t * icmp_packet = (uint8_t *)malloc(len);
  struct sr_ethernet_hdr *new_packet_eth_hdr = (struct sr_ethernet_hdr *)icmp_packet;
  struct sr_ethernet_hdr *packet_eth_hdr = (struct sr_ethernet_hdr *)packet;
  struct sr_ip_hdr *new_packet_ip_hdr = (struct sr_ip_hdr *)(icmp_packet + sizeof(sr_ethernet_hdr_t));
  struct sr_ip_hdr *packet_ip_hdr = (struct sr_ip_hdr *)(packet + sizeof(sr_ethernet_hdr_t));
  struct sr_icmp_hdr *new_packet_icmp_hdr = (struct sr_icmp_hdr *)(icmp_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  memset(new_packet_icmp_hdr, 0, sizeof(sr_icmp_hdr_t));

  //Set the ethernet header fields
  memcpy(new_packet_eth_hdr->ether_dhost, packet_eth_hdr->ether_shost, ETHER_ADDR_LEN);
  memcpy(new_packet_eth_hdr->ether_shost, sr_get_interface(sr, interface)->addr, ETHER_ADDR_LEN);
  new_packet_eth_hdr->ether_type = htons(ethertype_ip);

  //Set the IP header fields
  memcpy(new_packet_ip_hdr, packet_ip_hdr, sizeof(sr_ip_hdr_t));
  new_packet_ip_hdr->ip_len = htons(len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
  new_packet_ip_hdr->ip_id = htons(packet_ip_hdr->ip_id);

  new_packet_ip_hdr->ip_off = htons(IP_DF);

  new_packet_ip_hdr->ip_ttl = 255;
  new_packet_ip_hdr->ip_p = ip_protocol_icmp;
  new_packet_ip_hdr->ip_sum = 0;
  new_packet_ip_hdr->ip_src = sr_get_interface(sr, interface)->ip;
  new_packet_ip_hdr->ip_dst = htonl(packet_ip_hdr->ip_src);
  new_packet_ip_hdr->ip_sum = cksum(new_packet_ip_hdr, sizeof(sr_ip_hdr_t));

  //Set the ICMP header fields
  if (type == 0) {
    struct sr_icmp_hdr *packet_icmp_hdr = (struct sr_icmp_hdr *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    memcpy(new_packet_icmp_hdr, packet_icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
    new_packet_icmp_hdr->icmp_type = 0;
    new_packet_icmp_hdr->icmp_sum = 0;
  } else {
    new_packet_icmp_hdr->icmp_type = type;
    new_packet_icmp_hdr->icmp_code = code;
    new_packet_icmp_hdr->icmp_sum = 0;
    new_packet_icmp_hdr->unused = 0;
    memcpy(new_packet_icmp_hdr->data, packet_ip_hdr, ICMP_DATA_SIZE);
  }

  icmp_hdr_to_network(new_packet_icmp_hdr);
  if (type != 0) {
    new_packet_icmp_hdr->icmp_sum = cksum(new_packet_icmp_hdr, sizeof(sr_icmp_hdr_t));
  } else {
    new_packet_icmp_hdr->icmp_sum = cksum(new_packet_icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
  }
  sr_send_packet(sr, icmp_packet, len, interface);

  free(icmp_packet);
}

void send_arpreq(struct sr_instance* sr,
        unsigned int len,
        char* interface/* lent */,
        struct sr_arpreq *request) {
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

  sr_send_packet(sr, arp_request, sizeof(sr_arp_hdr_t) + sizeof(sr_ethernet_hdr_t), iface->name);

  free(arp_request);
  return;
}

struct sr_rt *sr_longest_prefix_match(struct sr_instance *sr, uint32_t dest_ip) {
  struct sr_rt *rt_ptr = sr->routing_table;
  struct sr_rt *longest_prefix_match;
  uint32_t longest_mask = 0;

  while(rt_ptr) {
    if ((dest_ip & ntohl(rt_ptr->mask.s_addr)) == ntohl(rt_ptr->dest.s_addr)) {
      if (ntohl(rt_ptr->mask.s_addr) > ntohl(longest_mask)) {
        longest_mask = rt_ptr->mask.s_addr;
        longest_prefix_match = rt_ptr;
      }
    }
    rt_ptr = rt_ptr->next;
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