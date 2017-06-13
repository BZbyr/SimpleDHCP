#ifndef _PACKET_H
#define _PACKET_H

#include <netinet/udp.h>
#include <netinet/ip.h>

struct dhcpMessage {
        u_int8_t op;            //(1)报文类型, 1表示请求报文, 2表示回应报文
	u_int8_t htype;         //(1)硬件地址类型,1表示10Mb/s的以太网的硬件地址
	u_int8_t hlen;          //(1)硬件地址长度, 以太网中该值是6
	u_int8_t hops;          //(1)条数, client端设置为0, 也能被一个代理服务器设置
	u_int32_t xid;          //(4)事务ID，由client选择的一个随机数，被server和client用来交流request和reply，client用它对request和reply进行匹配。该ID由client设置并由server返回，为32位整数。DHCPREQUEST 时产生的数据,以作为DHCPREPLY的数据
	u_int16_t secs;         //(2)由客户端填充，表示从客户端开始获得IP地址或IP地址续借后所使用了的秒数,client端启动时间
	u_int16_t flags;        //(2)标准位,这个16比特的字段，只使用0位比特，0表示采用单播发送方式，1表示广播发送方式。最左1bit为1时表示server将以广播方式传送封包给client
	u_int32_t ciaddr;       //(4)client端的IP地址。只有客户端是Bound、Renew、Rebinding状态，并且能响应ARP请求时，才能被填充。要是client想继续使用之前取得地址,则列于这里.
	u_int32_t yiaddr;	//(4)DHCP服务器分配给客户端的IP地址。仅在DHCP服务器发送的Offer和ACK报文中显示，其他报文中显示为0. 在DHCPOFFER和DHCPACK中,这里表示client的ip地址
	u_int32_t siaddr;	//(4)表明DHCP协议流程的下一个阶段要使用的服务器的IP地址。下一个为DHCPclient分配IP地址等信息的DHCP服务器IP地址。仅在DHCP Offer、DHCP ACK报文中显示，其他报文中显示为0。(用于bootstrap过程中的IP地址)用于网络开机
	u_int32_t giaddr;	//DHCPclient发出request报文后经过的第一个DHCP中继的IP地址。如果没有经过DHCP中继，则显示为0。(转发代理（网关）IP地址)*跨网络的dhcp发放时,这里用来保存relayagent地址. 注意：不是地址池中定义的网关
	u_int8_t chaddr[16];	//client端MAC地址, client必须设置它的"chaddr"字段。UDPpacket中的以太网帧首部也有该字段，但通常通过查看UDPpacket确定以太网帧首部中的该字段获取该值比较困难或者说不可能，而在UDP协议承载的DHCP报文中设置该字段，用户进程就可以很容易地获取该值。
	u_int8_t sname[64];	//server之名称字窜, 该字段是空结尾的字符串0x00，由server填写,在Offer和ACK报文中显示发送报文的DHCP服务器名称，其他报文显示为0。
	u_int8_t file[128];	//启动文件名，是一个空结尾的字符串。DHCPserver为DHCPclient指定的启动配置文件名称及路径信息。DHCP Offer报文中提供有效的目录路径全名。仅在DHCP Offer报文中显示，其他报文中显示为空。
	u_int32_t cookie;
	u_int8_t options[308]; 	//格式为"代码+长度+数据"
};

struct udp_dhcp_packet { //定义来一个UDP包
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
