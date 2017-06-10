/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   options.h
 * Author: bzbyr
 *
 * Created on 2017年6月9日, 下午5:08
 */

#ifndef OPTIONS_H
#define OPTIONS_H

#include "packet.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *get_option(struct dhcpMessage *packet, int code);


#ifdef __cplusplus
}
#endif

#endif /* OPTIONS_H */

