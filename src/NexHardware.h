// Manually pulled off the Nextion library and modified

/**
 * @file NexHardware.h
 *
 * The definition of base API for using Nextion.
 *
 * @author  Wu Pengfei (email:<pengfei.wu@itead.cc>)
 * @date    2015/8/11
 * @copyright
 * Copyright (C) 2014-2015 ITEAD Intelligent Systems Co., Ltd. \n
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */
#ifndef __NEXHARDWARE_H__
#define __NEXHARDWARE_H__
#include <Arduino.h>

/**
 * Define DEBUG_SERIAL_ENABLE to enable debug serial.
 * Comment it to disable debug serial.
 */
#define DEBUG_SERIAL_ENABLE

/**
 * Define dbSerial for the output of debug messages.
 */
#define dbSerial Serial

#ifdef DEBUG_SERIAL_ENABLE
#define dbSerialPrint(a) dbSerial.print(a)
#define dbSerialPrintln(a) dbSerial.println(a)
#define dbSerialBegin(a) dbSerial.begin(a)
#else
#define dbSerialPrint(a) \
	do {                 \
	} while (0)
#define dbSerialPrintln(a) \
	do {                   \
	} while (0)
#define dbSerialBegin(a) \
	do {                 \
	} while (0)
#endif

#define NEX_RET_CMD_FINISHED (0x01)
#define NEX_RET_EVENT_LAUNCHED (0x88)
#define NEX_RET_EVENT_UPGRADED (0x89)
#define NEX_RET_EVENT_TOUCH_HEAD (0x65)
#define NEX_RET_EVENT_POSITION_HEAD (0x67)
#define NEX_RET_EVENT_SLEEP_POSITION_HEAD (0x68)
#define NEX_RET_CURRENT_PAGE_ID_HEAD (0x66)
#define NEX_RET_STRING_HEAD (0x70)
#define NEX_RET_NUMBER_HEAD (0x71)
#define NEX_RET_INVALID_CMD (0x00)
#define NEX_RET_INVALID_COMPONENT_ID (0x02)
#define NEX_RET_INVALID_PAGE_ID (0x03)
#define NEX_RET_INVALID_PICTURE_ID (0x04)
#define NEX_RET_INVALID_FONT_ID (0x05)
#define NEX_RET_INVALID_BAUD (0x11)
#define NEX_RET_INVALID_VARIABLE (0x1A)
#define NEX_RET_INVALID_OPERATION (0x1B)

/**
 * @addtogroup CoreAPI
 * @{
 */

/**
 * @}
 */

template <class SerialT>
auto recvRetNumber(SerialT& nexSerial, uint32_t* number, uint32_t timeout = 100)
	-> bool {
	bool ret = false;
	uint8_t temp[8] = {0};

	if (!number) {
		goto __return;
	}

	nexSerial.setTimeout(timeout);
	if (sizeof(temp) != nexSerial.readBytes((char*)temp, sizeof(temp))) {
		goto __return;
	}

	if (temp[0] == NEX_RET_NUMBER_HEAD && temp[5] == 0xFF && temp[6] == 0xFF &&
		temp[7] == 0xFF) {
		*number = ((uint32_t)temp[4] << 24) | ((uint32_t)temp[3] << 16) |
				  (temp[2] << 8) | (temp[1]);
		ret = true;
	}

__return:

	if (ret) {
		dbSerialPrint("recvRetNumber :");
		dbSerialPrintln(*number);
	} else {
		dbSerialPrintln("recvRetNumber err");
	}

	return ret;
}

template <class SerialT>
auto recvRetString(SerialT& nexSerial,
				   char* buffer,
				   uint16_t len,
				   uint32_t timeout = 100) -> uint16_t {
	uint16_t ret = 0;
	bool str_start_flag = false;
	uint8_t cnt_0xff = 0;
	String temp = String("");
	uint8_t c = 0;
	long long start;

	if (!buffer || len == 0) {
		goto __return;
	}

	start = millis();
	while (millis() - start <= timeout) {
		while (nexSerial.available()) {
			c = nexSerial.read();
			if (str_start_flag) {
				if (0xFF == c) {
					cnt_0xff++;
					if (cnt_0xff >= 3) {
						break;
					}
				} else {
					temp += (char)c;
				}
			} else if (NEX_RET_STRING_HEAD == c) {
				str_start_flag = true;
			}
		}

		if (cnt_0xff >= 3) {
			break;
		}
	}

	ret = temp.length();
	ret = ret > len ? len : ret;
	strncpy(buffer, temp.c_str(), ret);

__return:

	dbSerialPrint("recvRetString[");
	dbSerialPrint(temp.length());
	dbSerialPrint(",");
	dbSerialPrint(temp);
	dbSerialPrintln("]");

	return ret;
}

template <class SerialT>
auto recvRetCommandFinished(SerialT& nexSerial, uint32_t timeout = 100)
	-> bool {
	bool ret = false;
	uint8_t temp[4] = {0};

	nexSerial.setTimeout(timeout);
	if (sizeof(temp) != nexSerial.readBytes((char*)temp, sizeof(temp))) {
		ret = false;
	}

	if (temp[0] == NEX_RET_CMD_FINISHED && temp[1] == 0xFF && temp[2] == 0xFF &&
		temp[3] == 0xFF) {
		ret = true;
	}

	if (ret) {
		dbSerialPrintln("recvRetCommandFinished ok");
	} else {
		dbSerialPrintln("recvRetCommandFinished err");
	}

	return ret;
}

#endif /* #ifndef __NEXHARDWARE_H__ */
