# README for Assignment 2: Router

Name: Casey Burhoe

JHED: cburhoe1

---

**DESCRIBE YOUR CODE AND DESIGN DECISIONS HERE**

This will be worth 10% of the assignment grade.

Some guiding questions:
- What files did you modify (and why)?
- What helper method did you write (and why)?
- What logic did you implement in each file/method?
- What problems or challenges did you encounter?

I wrote all of my code in sr_router.c/h and sr_arpcache.c/h .

I wrote a number of helper methods which are all located in sr_router.c/h . These had various functionality; for instance, sr_longest_prefix_match() 
would determine the longest matching prefix and therefore where to forward a packet. there were a number of helper functions meant to convert
the headers of each buffer into their host or network byte orders, but I ended up not using them much as bugs arose in my code. sanity_check()
was my function to check the length and checksum of an ip packet header. for_us() would determine if a packet was destined for any of the router's
interfaces. Finally, I had functions for handling forwarding IP packets, sending ARP packets, and sending ICMP packets, which would
take a packet and some other relevant information and construct the headers necessary for sending these packets.
I wrote these helper functions to help break down the router problem into smaller steps to become more easily manageable.

The biggest issue I ran into was constructing the ICMP packets. The instructions and documentation are very unclear about what to set
each of the header fields to, and what data to include. I also found a blatant error in the assignment instructions which said to 
not set the fragmentation offset field of an ICMP echo response IP header to IP_DF, when that actually made my echo response packets send successfully.
I believe the source of my issues lies in the calculation of the checksum or the header lengths for ICMP packets. Upon inspection, all of my packets
look well formed. Therefore, it must be the case that the checksums or the lengths are incorrect, which is impossible to tell with the provided instructions.

Another hurdle I faced was the byte ordering of my packet fields. Originally, I neglected to convert between host and network byte ordering at all,
which led to my router not having any functionality. This is what led me to make the header struct byte order conversion helper functions.
