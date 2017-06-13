/* dhcpd.c
 *
 * udhcp Server
 * Copyright (C) 1999 Matthew Ramsay <matthewr@moreton.com.au>
 *			Chris Trew <ctrew@moreton.com.au>
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 整个dhcp server运行的主线，server开始运行是从main函数开始，
 * 相当于我们程序的main入口。在main将各个功能模块组合起来
 * 实现我们的dhcp server。
 */

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>

#include "debug.h"
#include "dhcpd.h"
#include "arpping.h"
#include "socket.h"
#include "options.h"
#include "files.h"
#include "leases.h"
#include "packet.h"
#include "serverpacket.h"
#include "pidfile.h"


/* globals */
struct dhcpOfferedAddr *leases;
struct server_config_t server_config;
static int signal_pipe[2];

/* Exit and cleanup */
static void exit_server(int retval)
{
	pidfile_delete(server_config.pidfile);
	CLOSE_LOG();
	exit(retval);
}


/* Signal handler */
static void signal_handler(int sig)
{
	if (send(signal_pipe[1], &sig, sizeof(sig), MSG_DONTWAIT) < 0) {
		LOG(LOG_ERR, "Could not send signal: %s", 
			strerror(errno));
	}
}


#ifdef COMBINED_BINARY	
int udhcpd_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{	
	fd_set rfds;
	struct timeval tv;
	int server_socket = -1;
	int bytes, retval;
	struct dhcpMessage packet; // 声明函数包
	unsigned char *state;
	unsigned char *server_id, *requested;
	u_int32_t server_id_align, requested_align;
	unsigned long timeout_end;
	struct option_set *option;
	struct dhcpOfferedAddr *lease;
	int pid_fd;
	int max_sock;
	int sig;
	
	OPEN_LOG("udhcpd");
	printf("udhcp server (v%s) started");

	memset(&server_config, 0, sizeof(struct server_config_t));
	
	if (argc < 2)
		read_config(DHCPD_CONF_FILE);
	else read_config(argv[1]);

	pid_fd = pidfile_acquire(server_config.pidfile);
	pidfile_write_release(pid_fd);

	if ((option = find_option(server_config.options, DHCP_LEASE_TIME))) { /* find option DHCP_LEASE_TIME 'code' in opt_list */
		memcpy(&server_config.lease, option->data + 2, 4);
		server_config.lease = ntohl(server_config.lease);
	}
	else server_config.lease = LEASE_TIME;
	
	leases = malloc(sizeof(struct dhcpOfferedAddr) * server_config.max_leases);
	memset(leases, 0, sizeof(struct dhcpOfferedAddr) * server_config.max_leases);//按照结构体*数目来的，变成leases[]了
	read_leases(server_config.lease_file);
        
        //读服务器本身的地址
	if (read_interface(server_config.interface, &server_config.ifindex,
			   &server_config.server, server_config.arp) < 0)
		exit_server(1);

#ifndef DEBUGGING
	pid_fd = pidfile_acquire(server_config.pidfile); /* hold lock during fork. */
	if (daemon(0, 0) == -1) {
		perror("fork");
		exit_server(1);
	}
	pidfile_write_release(pid_fd);
#endif


	socketpair(AF_UNIX, SOCK_STREAM, 0, signal_pipe); //  创建TCP通信，实现双工通信
	signal(SIGUSR1, signal_handler);
	signal(SIGTERM, signal_handler);

	timeout_end = time(0) + server_config.auto_time;
	while(1) { /* loop until universe collapses */

                //一下使 关于连接的建立和监听
		if (server_socket < 0)
			if ((server_socket = listen_socket(INADDR_ANY, SERVER_PORT, server_config.interface)) < 0) {
				LOG(LOG_ERR, "FATAL: couldn't create server socket, %s", strerror(errno));
				exit_server(0);
			}			

		FD_ZERO(&rfds); //清空集合中所有元素
		FD_SET(server_socket, &rfds); //设置server socket，并使集合包含
		FD_SET(signal_pipe[0], &rfds);
		if (server_config.auto_time) {
			tv.tv_sec = timeout_end - time(0);
			tv.tv_usec = 0;
		}
                
                /* 如果auto_time为0，或tv_sec大于0时，建立select等待server_socket和signal_pipe的信号 */
		if (!server_config.auto_time || tv.tv_sec > 0) {
			max_sock = server_socket > signal_pipe[0] ? server_socket : signal_pipe[0];
                         /* 对两个fd都进行可读性检测 */
                        
			retval = select(max_sock + 1, &rfds, NULL, NULL, server_config.auto_time ? &tv : NULL);
                        /*如果auto_time不为0,则非阻塞，超时时间为上述的剩余时间；如果为0，则time设为NULL，select将一直阻塞直到某个fd上接收到信号*/
		} else retval = 0; /* If we already timed out, fall through */
                
                /* 若直到超时都没有接收到信号，则立即写lease文件，并更新time_end */
		if (retval == 0) {
			write_leases();
			timeout_end = time(0) + server_config.auto_time;
			continue;
		} else if (retval < 0 && errno != EINTR) {
			DEBUG(LOG_INFO, "error on select");
			continue;
		}
		
                /* 若signal_pipe接收到可读signal（signal_handler将signal写入signal_pipe[1],根据socketpair的特性，此时signal_pipe[0]将可读，产生一个可读的信号） */
                //就是 一个读 一个写
		if (FD_ISSET(signal_pipe[0], &rfds)) {
			if (read(signal_pipe[0], &sig, sizeof(sig)) < 0)
				continue; /* probably just EINTR */
			switch (sig) {
			case SIGUSR1:
                            /* 接收到SIGUSR1，立即写leases，并更新time_end */
				LOG(LOG_INFO, "Received a SIGUSR1");
				write_leases();
				/* why not just reset the timeout, eh */
				timeout_end = time(0) + server_config.auto_time;
				continue;
			case SIGTERM:
				LOG(LOG_INFO, "Received a SIGTERM");
				exit_server(0);
			}
		}
                
                // 一下使关于获取和提取报文。
                /*
                 * 调用get_packet()函数从server_socket接收数据报文填充到packet，
                 * 注意该函数在调用read读取报文之后，会对报文内的数据进行简单校验，
                 * 包括cookie（若不为DHCP_MAGIC，则丢弃），
                 * 以及根据op和options的vender字段判断是否强制设定flag字段为1（广播）。
                 */ 
		if ((bytes = get_packet(&packet, server_socket)) < 0) { /* this waits for a packet - idle */
			if (bytes == -1 && errno != EINTR) {
				DEBUG(LOG_INFO, "error on read, %s, reopening socket", strerror(errno));
				close(server_socket);
				server_socket = -1;
			}
			continue;
		}
                /*
                 * 使用get_option函数提取state状态信息，
                 * gei_option函数是从packet的options字段中，
                 * 基于CLV（1+1+n）格式，提取出指定的option的信息，
                 * 返回该选项的value值对应的指针。
                 */
		if ((state = get_option(&packet, DHCP_MESSAGE_TYPE)) == NULL) {
			DEBUG(LOG_ERR, "couldn't get option from packet, ignoring");
			continue;
		}
		
		/* ADDME: look for a static lease */
		lease = find_lease_by_chaddr(packet.chaddr);
                
                /*根据state和packet内容响应报文*/
		switch (state[0]) {
                    case DHCPDISCOVER:
                        /* 
                         * 因为服务器只有在DHCP客户端处于初始化的时候，INIT_SELECTING状态
                         * 才会接收到DHCPDISCOVER消息，
                         * 因此根据本地lease列表和报文中MAC地址的情况，
                         * 直接回复DHCPOFFER消息
                         */
                            DEBUG(LOG_INFO,"received DISCOVER");
                            /*基于接收的packet发送DHCPOFFER消息*/
                            
                            if (sendOffer(&packet) < 0) {
                                    LOG(LOG_ERR, "send OFFER failed");
                            }
                            break;
                    case DHCPREQUEST:
                        /*
                         * 服务器在接收到REQUEST消息时的处理比较复杂，
                         * 因为客户端可以在多种情况下发送DHCPREQUEST消息，
                         * 比如SELECTING状态、INIT_REBOOTING状态、RENEWING
                         * or REBINDING状态，三类状态在options字段有明显的区别
                         */
                            
                            DEBUG(LOG_INFO, "received REQUEST");
                            
                            /*client 端选定的IP*/
                            requested = get_option(&packet, DHCP_REQUESTED_IP); 
                            server_id = get_option(&packet, DHCP_SERVER_ID);

                            if (requested) 
                                memcpy(&requested_align, requested, 4);
                            if (server_id) 
                                memcpy(&server_id_align, server_id, 4);
                            /*
                             * lease不为NULL，即在本地有该client的记录。
                             * 即便是从INIT过来的client，
                             * 在接收到它的DHCPDISCOVER并发送OFFER之后，也在本地做了记录
                             */
                            if (lease) {
                                /* 
                                 * ADDME: or static lease 
                                 * DHCPREQUEST中，server_id若不为0，
                                 * 则是SELECTING State
                                 * （该选项在client从DISCOVER，到收到OFFER后，
                                 * 回复的REQUEST中必须包含，指定选中的server，
                                 * 在其他情况下不需要包含）
                                 */
                                if (server_id) {
                                    /* SELECTING State */
                                    DEBUG(LOG_INFO, "server_id = %08x", ntohl(server_id_align));

                                    /*
                                     * SELECTING状态下，校验server_id和本地server匹配，
                                     * request_ip存在且和lease分配的yiaddr一致，则发送ACK
                                     */
                                    if (server_id_align == server_config.server && requested && 
                                        requested_align == lease->yiaddr) {
                                            sendACK(&packet, lease->yiaddr);
                                    }
                                    /* server_id为0，是其他状态 */
                                } else {
                                    if (requested) {
                                            /* INIT-REBOOT State 
                                             * 如果请求的IP和分配的IP一致，
                                             * 且没有被分配给其他主机, 
                                             * 发送ACK，否则发送NAK
                                             */
                                            if (lease->yiaddr == requested_align)
                                                    sendACK(&packet, lease->yiaddr);
                                            else sendNAK(&packet);
                                    } else {
                                            /* RENEWING or REBINDING State
                                             * 如果分配的yiaddr和客户端IP一致，
                                             * 即请求RENEWING或REBINDING的主机确实和本地有关联，
                                             * 则继续，进一步校验IP是否已被分配
                                             */
                                            if (lease->yiaddr == packet.ciaddr)
                                                    sendACK(&packet, lease->yiaddr);
                                            else {
                                                    /* don't know what to do!!!! */
                                                    sendNAK(&packet);
                                            }
                                    }						
                                }

                                
                            } else if (server_id) {
                                /* what to do if we have no record of the client
                                 * 一个没有任何记录却收到其REQUEST消息的client，
                                 * 可以认为是一个错误或者恶意攻击，
                                 * 一般对其进行静默处理或者设置黑户
                                 * SELECTING State 
                                 * 若server_id不为0，则处于SELECTING State，
                                 * 则该REQUEST消息可能是来自其他未收到其广播的DISCOVER消息的client，
                                 * 或意外接收到其他网段的消息，采取静默处理
                                 */

                            } else if (requested) {
                                /* 
                                 * INIT-REBOOT State
                                 * 若携带requested_ip，则处于INIT-REBOOT State，
                                 * 若该IP在本地lease列表中存在，且lease已超时，
                                 * 则丢弃该lease，否则直接发送NAK
                                 */
                                if ((lease = find_lease_by_yiaddr(requested_align))) {
                                        if (lease_expired(lease)) {
                                            /* probably best if we drop this lease */
                                            memset(lease->chaddr, 0, 16);
                                        /* make some contention for this address */
                                        } else 
                                            sendNAK(&packet);
                                } else if (requested_align < server_config.start || 
                                           requested_align > server_config.end) {
                                        sendNAK(&packet);
                                } /* else remain silent */

                            } else {
                                     /* RENEWING or REBINDING State */
                            }
                            break;
                    case DHCPDECLINE:
                        /*
                         * 收到DHCPDECLINE消息，则丢弃本地记录的lease：
                         * 设置chaddr为0，即清除对该client的记录，
                         * 并且设置该lease在decline_time后
                         */
                        DEBUG(LOG_INFO,"received DECLINE");
                        if (lease) {
                                memset(lease->chaddr, 0, 16);
                                lease->expires = time(0) + server_config.decline_time;
                        }			
                        break;
                    case DHCPRELEASE:
                        /*
                         * 服务器在接收到客户端DHCPRELEASE消息后，
                         * 直接设置本地lease超时，但是并未清除该client的记录，
                         * 即下一次该客户端可以跳过DISCOVER，直接获取IP配置。
                         */
                        DEBUG(LOG_INFO,"received RELEASE");
                        if (lease) lease->expires = time(0);
                        break;
                    case DHCPINFORM:
                        
                        /*
                         * 客户端在已经有外部配置网络地址时，
                         * 发送DHCPINFORM只为了获取本地配置参数。
                         * 服务器接收后，直接发送INFORM数据包、
                         */
                        
                        DEBUG(LOG_INFO,"received INFORM");
                        send_inform(&packet);
                        break;	
                    default:
                            LOG(LOG_WARNING, "unsupported DHCP message (%02x) -- ignoring", state[0]);
		}
	}

	return 0;
}

