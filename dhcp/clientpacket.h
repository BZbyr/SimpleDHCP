/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   clientpacket.h
 * Author: bzbyr
 *
 * Created on 2017年6月12日, 下午5:04
 */

#ifndef CLIENTPACKET_H
#define CLIENTPACKET_H

#ifdef __cplusplus
extern "C" {
#endif



unsigned long random_xid(void);
int send_discover(unsigned long xid, unsigned long requested);
int send_selecting(unsigned long xid, unsigned long server, unsigned long requested);
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_release(unsigned long server, unsigned long ciaddr);
int get_raw_packet(struct dhcpMessage *payload, int fd);





#ifdef __cplusplus
}
#endif

#endif /* CLIENTPACKET_H */

