/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* get an option with bounds checking (warning, not aligned). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dhcpd.h"
/*  
 * 参数struct dhcpMessage *packet DHCP报文  
 * int code需要获得什么选项信息(选项信息的标识) 
 * 
 * 返回指向选项信息的指针（去除了 OPT_CODE,OPT_LEN） 
 * 未找到返回NULL 
 */  
unsigned char *get_option(struct dhcpMessage *packet, int code){
	int i, length;
	unsigned char *optionptr;
	int over = 0, done = 0, curr = OPTION_FIELD;
	
	optionptr = packet->options;
	i = 0;
	length = 308; /* 整个options字段的长度308 */
        
         /* 在options字段里查找code选项标识信息*/  
	while (!done) {
            
            /*
             * CLV方式存储数据  
             *这里与struct option_set的data存储相似  
             *OPT_CODE字节上存储code标识  
             *OPT_LEN 字节上存储信息长度  
             *OPT_LEN后就是存储信息
             */
            if (optionptr[i + OPT_CODE] == code) {
                    return optionptr + i + 2;// 
            }
            switch (optionptr[i + OPT_CODE]) {
                
            /* 
             * 不是填充信息:DHCP_PADDING 
             * 选项过载:DHCP_OPTION_OVER 
             * 选项结束:DHCP_END
             */  
                case DHCP_PADDING:  //DHCP_PADING(填充)字节,直接 i++;
                        i++;
                        break;
                case DHCP_OPTION_OVER:  //选项过载DHCP_OPTION_OVER
                        over = optionptr[i + 3];
                        /* 
                         *optionptr[i + OPT_CODE] == DHCP_OPTION_OVER选项过载; 
                         *  optionptr[i + 3]存放了采用哪个字段来存储过载的选项 
                         *   可能存储过载选项的字段: 
                         *       uint8_t sname[64]; //server host name (ASCIZ)  
                         *       uint8_t file[128];  // boot file name (ASCIZ)  
                         *  
                         *  over = optionptr[i + 3];记录下使用那个字段存储过载选项 
                         */  
                        i += optionptr[OPT_LEN] + 2;
                        break;
                case DHCP_END:  //选项字段结束标志 DHCP_END 0xff

                    /*  
                     * 当选项过载的时候(curr == OPTION_FILE允许选项过载) 
                     *  首先用file字段,不够的话再用sname字段 
                     *  使用file字段的时候: 
                     *      over的右起的第0位必须为1 
                     *  使用sname字段: 
                     *      over的右起的第一位必须为1 
                     */  
                        if (curr == OPTION_FIELD && over & FILE_FIELD) {
                                optionptr = packet->file;
                                i = 0;
                                length = 128;
                                curr = FILE_FIELD;
                        } else if (curr == FILE_FIELD && over & SNAME_FIELD) {
                                optionptr = packet->sname;
                                i = 0;
                                length = 64;
                                curr = SNAME_FIELD;
                                //没有或不允许选项过载或over(options[i + 3])标志不允许，结束查找返回NULL
                        } else done = 1;
                        break;
            /*  表明是属于选项信息,所以可以直接改变偏移量: 
             *  i += option[OPT_LEN + i] + 2;
             */ 
            default:
                    i += optionptr[OPT_LEN + i] + 2;
            }
	}
	return NULL;
}