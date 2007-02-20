/*
    This file is part of openrrcp

    OpenRRCP is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    OpenRRCP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenRRCP; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    ---

    You can send your updates, patches and suggestions on this software
    to it's original author, Andrew Chernyak (nording@yandex.ru)
    This would be appreciated, however not required.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#else
#include <net/if.h>
#include <pcap.h>
#include <err.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef htonl
#include <arpa/inet.h>
#endif

#ifndef __linux__
#include <dnet.h>
#endif

#include <errno.h>
#include "rrcp_packet.h"
#include "rrcp_io.h"
#include "rrcp_switches.h"

char ifname[128] = "";
uint16_t authkey = 0x2379;
unsigned char my_mac[6] = {0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
unsigned char dest_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char mac_bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

unsigned int switchtype;

int s,s_rec,s_send;

#ifdef __linux__
struct sockaddr_ll sockaddr_rec,sockaddr_send;
#else
eth_t                    *p_eth;
pcap_t                   *handle;
const u_char		 *rcvbuf;
struct addr               intf_mac;
char errbuf[PCAP_ERRBUF_SIZE];
struct bpf_program filter;
bpf_u_int32 mask;
bpf_u_int32 net;
char filter_app[512];
#endif

// takes logical port number as printed on switch' case (1,2,3...)
// returns physical port number (0,1,2...) or -1 if this device has no such logical port
int map_port_number_from_logical_to_physical(int port){
    return switchtypes[switchtype].port_order[port-1]-1;
}

int map_port_number_from_physical_to_logical(int port){
    int i;
    for (i=1;i<=switchtypes[switchtype].num_ports;i++){
	if(port==(switchtypes[switchtype].port_order[i-1]-1)){
	    return i;
	}
    }
    return -1;
}

#ifdef __linux__
void rtl83xx_prepare(){
    struct ifreq ifr;

    s_rec = socket(PF_PACKET, SOCK_RAW, htons(0x8899));
    if (s_rec == -1) { printf("can't create raw socket for recieve!\nAre we are running as root (uid=0)?\n"); exit(0); }
    s_send = socket(PF_PACKET, SOCK_RAW, htons(0x8899));
    if (s_send == -1) { printf("can't create raw socket for send!\nAre we are running as root (uid=0)?\n"); exit(0); }

    sockaddr_send.sll_family   = PF_PACKET;	
    sockaddr_send.sll_protocol = htons(0x8899);
    sockaddr_send.sll_ifindex  = if_nametoindex(ifname);
    sockaddr_send.sll_hatype   = ARPHRD_ETHER;
    sockaddr_send.sll_pkttype  = PACKET_OTHERHOST;
    sockaddr_send.sll_halen    = 6;

    memcpy(&sockaddr_rec,&sockaddr_send,sizeof(sockaddr_rec));

    /*Get source MAC*/
    memset(&ifr, 0x00, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if(ioctl(s_rec, SIOCGIFHWADDR, &ifr) < 0){
	printf("Fail to get hw addr\nAre we are running as root (uid=0)?\n");
	exit(0);
    }
    memcpy(my_mac,ifr.ifr_hwaddr.sa_data,6);
}

//send to wire
ssize_t sock_send(void *ptr, int size){
    int i,res;
    for (i=0;i<3;i++){
	res=sendto(s_send, ptr, size, 0, (struct sockaddr*)&sockaddr_send, sizeof(sockaddr_send));
	if (res!=-1) return res;
	usleep(50000);
    }
    printf("can't sendto!");
    _exit(1);
}

//recieve from wire, returns length
int sock_rec_(void *ptr, int size, int waittick){
    int i,res=0,len=0;
    for (i=0;i<10;i++){
	res=recvfrom(s_rec, ptr, size, MSG_DONTWAIT, (struct sockaddr*)&sockaddr_rec, (unsigned int *)&len);
	if (res==-1){
	    usleep(waittick);
	}else{
	    break;
	}
    }
    return len;
}

#else
void rtl83xx_prepare(){

 intf_mac.addr_type=ADDR_TYPE_ETH;
 intf_mac.addr_bits=ETH_ADDR_BITS;
 if ((p_eth = eth_open(ifname)) == NULL) errx(2, "eth_open");
 if (eth_get(p_eth,&intf_mac.addr_eth) < 0) errx(2, "get intf MAC");
 memcpy(my_mac,&intf_mac.addr_eth,ETH_ADDR_LEN);
 memset(filter_app,0,sizeof(filter_app));
 snprintf(filter_app, sizeof(filter_app), "ether proto 0x8899 and not ether src %s", addr_ntoa(&intf_mac));
 if (pcap_lookupnet(ifname, &net, &mask, errbuf) < 0){ net = 0;mask = 0;}
 if ((handle = pcap_open_live(ifname, 128, 0, 3, errbuf))== NULL) errx(2, "pcap_open_live: %s", errbuf);
#if defined(BSD) && defined(BIOCIMMEDIATE)
 {
  int on = 1;
  if (ioctl(pcap_fileno(handle), BIOCIMMEDIATE, &on) < 0) errx(2, "BIOCIMMEDIATE");
 }
#endif
 if (pcap_compile(handle, &filter, filter_app, 1, mask) < 0) errx(2, "bad pcap filter: %s", pcap_geterr(handle));
 if (pcap_setfilter(handle, &filter) < 0) errx(2, "bad pcap filter: %s", pcap_geterr(handle));
}

//send to wire
ssize_t sock_send(void *ptr, int size){
    int i,res;
    for (i=0;i<3;i++){
        res=eth_send(p_eth,ptr,size);
	if (res!=-1) return res;
	usleep(50000);
    }
    errx(2,"can't sendto!");
}

//recieve from wire, returns length
int sock_rec_(void *ptr, int size, int waittick){
    int len=0;
    int i=0;
    struct pcap_pkthdr pkt_h;
 
    while(i++ < 11){
     rcvbuf=pcap_next(handle,&pkt_h);
     if (rcvbuf!=NULL) break;
     usleep(waittick);     
    }
    if (rcvbuf!=NULL){
     if (pkt_h.caplen > size) len=size;
     else len=pkt_h.caplen;
     memcpy(ptr,rcvbuf,len);
    }
    return len;
}
#endif

int sock_rec(void *ptr, int size, int waittick){
    int len=0;
    len=sock_rec_(ptr, size, waittick);
    if (!len) {
       printf("can't recvfrom!\n");
       exit(1);
    }
    return len;
}

void rtl83xx_scan(int verbose){
    typedef struct sw_reply SW_REPLY;
    struct sw_reply { 
     struct rrcp_helloreply_packet_t pktr;
     SW_REPLY *prev;
    };
    int len = 0;
    int cnt_h_replies = 0;
    int cnt_r_replies = 0;
    int rep;
    struct rrcp_packet_t pkt;
    struct rrcp_helloreply_packet_t pktr;
    SW_REPLY *hello_reply=NULL;
    SW_REPLY *rep_reply=NULL;
    SW_REPLY *current=NULL;
    SW_REPLY *next_rep_reply;

    memcpy(pkt.ether_dhost,mac_bcast,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);

/* scan based on Hello packets */
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x00;
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);

    sock_send(&pkt, sizeof(pkt));

    usleep(1000);
    while(1){
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec_(&pktr, sizeof(pktr),5000);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x00 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(authkey)){
              if ( (hello_reply=malloc(sizeof(SW_REPLY))) == NULL ) { printf("malloc\n"); _exit(1); }
              memcpy(&hello_reply->pktr,&pktr,sizeof(pktr));
              hello_reply->prev=current;
              current=hello_reply;
	      cnt_h_replies++;
	} else break;
    }

/* scan based on REP packets */
    current=NULL;
    pkt.rrcp_proto=0x02;
    pkt.rrcp_opcode=0x00;
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=0x0000;

    sock_send(&pkt, sizeof(pkt));

    usleep(1000);
    while(1){
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec_(&pktr, sizeof(pktr),5000);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x02 &&
	    pktr.rrcp_opcode==0x00 &&
	    pktr.rrcp_isreply==1 ){
              if ( (rep_reply=malloc(sizeof(SW_REPLY))) == NULL ) { printf("malloc\n"); _exit(1); }
              memcpy(&rep_reply->pktr,&pktr,sizeof(pktr));
              rep_reply->prev=current;
              current=rep_reply;
	      cnt_r_replies++;
	} else break;
    }

/* print result */
    if (cnt_h_replies || cnt_r_replies){
       if (verbose){
  	   printf("  switch MAC/port        via MAC/port      vendor_id/chip_id REP\n");
       }else{
           printf("  switch MAC      Hello REP\n");
       }
    }else{
       printf("No switch found.\n");
       return;
    }
    if (cnt_h_replies){
       while (hello_reply != NULL){
                // We have REP reply from this switch?
                next_rep_reply=NULL;
                current=rep_reply;
                rep=0;
                while (current != NULL){
                 if (memcmp(&hello_reply->pktr.ether_shost,&current->pktr.ether_shost,6)==0){
                    rep++; 
                    if (next_rep_reply == NULL){
                      rep_reply=rep_reply->prev;
                    }else{
                      next_rep_reply->prev=current->prev;
                    }
                    free(current);
                    break;
                 }else{
                  next_rep_reply=current;
                  current=current->prev;
                 }
                }

		if (verbose){
		    printf("%02x:%02x:%02x:%02x:%02x:%02x/%-2d %02x:%02x:%02x:%02x:%02x:%02x/%-2d 0x%08x/0x%04x  %s\n",
			hello_reply->pktr.ether_shost[0],
			hello_reply->pktr.ether_shost[1],
			hello_reply->pktr.ether_shost[2],
			hello_reply->pktr.ether_shost[3],
			hello_reply->pktr.ether_shost[4],
			hello_reply->pktr.ether_shost[5],
			map_port_number_from_physical_to_logical(hello_reply->pktr.rrcp_downlink_port),
			hello_reply->pktr.rrcp_uplink_mac[0],
			hello_reply->pktr.rrcp_uplink_mac[1],
			hello_reply->pktr.rrcp_uplink_mac[2],
			hello_reply->pktr.rrcp_uplink_mac[3],
			hello_reply->pktr.rrcp_uplink_mac[4],
			hello_reply->pktr.rrcp_uplink_mac[5],
			map_port_number_from_physical_to_logical(hello_reply->pktr.rrcp_uplink_port),
			hello_reply->pktr.rrcp_vendor_id,
			hello_reply->pktr.rrcp_chip_id,
                        (rep)?"Yes":"No");
		}else{
		    printf("%02x:%02x:%02x:%02x:%02x:%02x   +    %c\n",
			hello_reply->pktr.ether_shost[0],
			hello_reply->pktr.ether_shost[1],
			hello_reply->pktr.ether_shost[2],
			hello_reply->pktr.ether_shost[3],
			hello_reply->pktr.ether_shost[4],
			hello_reply->pktr.ether_shost[5],
                        (rep)?'+':'-');
		}
            current=hello_reply->prev;
            free(hello_reply);
            hello_reply=current;
       }
    }
    if (cnt_r_replies){
       while (rep_reply != NULL){
                if (verbose){
                    printf("%02x:%02x:%02x:%02x:%02x:%02x    No hello-reply, RRCP disable?           Yes\n",
			rep_reply->pktr.ether_shost[0],
			rep_reply->pktr.ether_shost[1],
			rep_reply->pktr.ether_shost[2],
			rep_reply->pktr.ether_shost[3],
			rep_reply->pktr.ether_shost[4],
			rep_reply->pktr.ether_shost[5]);
                }else{
		    printf("%02x:%02x:%02x:%02x:%02x:%02x   -    +\n",
			rep_reply->pktr.ether_shost[0],
			rep_reply->pktr.ether_shost[1],
			rep_reply->pktr.ether_shost[2],
			rep_reply->pktr.ether_shost[3],
			rep_reply->pktr.ether_shost[4],
			rep_reply->pktr.ether_shost[5]);
                }
            current=rep_reply->prev;
            free(rep_reply);
            rep_reply=current;
       }
    }
}

uint32_t rtl83xx_readreg32(uint16_t regno){
    int len = 0;
    struct rrcp_packet_t pkt,pktr;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x01;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);
    pkt.rrcp_reg_addr=regno;
    pkt.rrcp_reg_data=0;

    sock_send(&pkt, sizeof(pkt));

    usleep(100);
    while(1){
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),100);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x01 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(authkey)&&
	    pktr.rrcp_reg_addr==regno){
	        return(pktr.rrcp_reg_data);
	}
    }
}

uint16_t rtl83xx_readreg16(uint16_t regno){
    return (uint16_t)rtl83xx_readreg32(regno);
}

void rtl83xx_setreg16(uint16_t regno, uint32_t regval){
    int cnt = 0;
    struct rrcp_packet_t pkt;
    uint16_t prev_auth=0;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x02;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);
    pkt.rrcp_reg_addr=regno;
    pkt.rrcp_reg_data=regval;

    for (cnt=0;cnt<3;cnt++){
	sock_send(&pkt, sizeof(pkt));
        if (!regno) return; // because register 0 self clearing 
        if (regno == 0x209) { // special hack for new authkey
           prev_auth=authkey; authkey=(uint16_t)regval; 
        } 
	if (rtl83xx_readreg32(regno)==regval){
	    return;
	}
        if (regno == 0x209) { authkey=prev_auth; } // revert authkey if unfinished change 
    }
//    printf("can't set register 0x%04x to value 0x%04lu (read value is 0x%04lu)\n",regno,regval,rtl83xx_readreg32(regno));
//    _exit(0);
}

void rtl83xx_setreg16reg16(uint16_t regno, uint16_t regval){
    rtl83xx_setreg16(regno, (uint32_t) regval);
}

int wait_eeprom(){
int i;
uint16_t res;

 for(i=0;i<10;i++){
  res=rtl83xx_readreg16(0x217);
  if ((res&0x1000) == 0) return(res);
  usleep(1000);
 }
 return(0xffff);
}

/* 
   Concerning algorithm of work with EEPROM the documentation contains a number of discrepancies.
   Correct data are resulted in tables 22 and 23 chapters 8.26.2 of description RTL8316b.

   register 0217h:
      bit 0-7  : EEPROM address
      bit 8-10 : Chip select (EEPROM = 0)
      bit 11   : Operation Read(1)/Write(0)
      bit 12   : Status Busy(1)/Idle(0)
      bit 13   : Operation success status Fail(1)/Success(0)
      bit 14-15 : reserved
   
   register 0218h:
      bit 0-7  : data written to EEPROM
      bit 8-15 : data read from EEPROM
*/

int do_write_eeprom(uint16_t addr,uint16_t data){

 rtl83xx_setreg16(0x218,data>>8);
 rtl83xx_setreg16(0x217,addr);
 if ((wait_eeprom()&0x2000)!=0) return 1;
 rtl83xx_setreg16(0x218,data&0x00ff);
 rtl83xx_setreg16(0x217,addr-1);
 if ((wait_eeprom()&0x2000)!=0) return 1;
 return 0;
}

int do_write_eeprom_byte(uint16_t addr,uint8_t data){
    rtl83xx_setreg16(0x218,((uint16_t)data)&0xff);
    rtl83xx_setreg16(0x217,addr);
    if ((wait_eeprom()&0x2000)!=0)
	return 1;
    return 0;
}

int do_read_eeprom(uint16_t addr,uint16_t *data){

 rtl83xx_setreg16(0x217,addr|0x800);
 if ((wait_eeprom()&0x2000)!=0) return 1;
 *data=rtl83xx_readreg16(0x218);
 rtl83xx_setreg16(0x217,(addr-1)|0x800);
 if ((wait_eeprom()&0x2000)!=0) return 1;
 *data|=rtl83xx_readreg16(0x218)>>8;
 return 0;
}

//returns 1 if got response
uint32_t rtl83xx_ping(void){
    int i,len = 0;
    struct rrcp_packet_t pkt,pktr;

    memcpy(pkt.ether_dhost,dest_mac,6);
    memcpy(pkt.ether_shost,my_mac,6);
    pkt.ether_type=htons(0x8899);
    pkt.rrcp_proto=0x01;
    pkt.rrcp_opcode=0x00;//0=hello; 1=get; 2=set
    pkt.rrcp_isreply=0;
    pkt.rrcp_authkey=htons(authkey);

    sock_send(&pkt, sizeof(pkt));

    for(i=1;i<20;i++){
	usleep(100*i);
	memset(&pktr,0,sizeof(pktr));
	len=sock_rec(&pktr, sizeof(pktr),100);
	if (len >14 &&
	    (memcmp(pktr.ether_dhost,my_mac,6)==0)&&
	    pktr.ether_type==htons(0x8899) &&
	    pktr.rrcp_proto==0x01 &&
	    pktr.rrcp_opcode==0x00 &&
	    pktr.rrcp_isreply==1 &&
	    pktr.rrcp_authkey==htons(authkey)){
	    return 1;
	}
    }
    return 0;
}
