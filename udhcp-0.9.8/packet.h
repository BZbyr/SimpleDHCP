#ifndef _PACKET_H
#define _PACKET_H

#include <netinet/udp.h>
#include <netinet/ip.h>

struct dhcpMessage {
	u_int8_t op;
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t hops;
	u_int32_t xid;/*DHCPREQUEST ʱ����������,����ΪDHCPREPLY������*/
	u_int16_t secs;/*client����ʱ��*/
	u_int16_t flags;
	u_int32_t ciaddr;/*Ҫ��client�����ʹ��֮ǰȡ�õ�ַ,����������*/
	u_int32_t yiaddr;/*��DHCPOFFER��DHCPACK��,�����ʾclient��ip��ַ*/
	u_int32_t siaddr;/*�������翪��*/
	u_int32_t giaddr;/*�������dhcp����ʱ,������������relayagent��ַ*/
	u_int8_t chaddr[16];/*clientӲ����ַ*/
	u_int8_t sname[64];/*server֮�����ִ�*/
	u_int8_t file[128];
	u_int32_t cookie;
	u_int8_t options[308]; /* 312 - cookie */ 
};

struct udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcpMessage data;
};

void init_header(struct dhcpMessage *packet, char type);
int get_packet(struct dhcpMessage *packet, int fd);
u_int16_t checksum(void *addr, int count);
int raw_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
		   u_int32_t dest_ip, int dest_port, unsigned char *dest_arp, int ifindex);
int kernel_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
		   u_int32_t dest_ip, int dest_port);


#endif
