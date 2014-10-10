/*
  stmflasher - Open Source ST MCU flash program for *nix
  Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>
  Copyright (C) 2011 Steve Markgraf <steve@steve-m.de>
  Copyright (C) 2012 Tormod Volden
  Copyright (C) 2012-2013 Alatar <alatar_@list.ru>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include <windows.h>

#include "serial.h"

struct serial {
	HANDLE fd;
	DCB oldtio;
	DCB newtio;

	char			configured;
	serial_baud_t		baud;
	serial_bits_t		bits;
	serial_parity_t		parity;
	serial_stopbit_t	stopbit;
};

serial_t* serial_open(const char *device) 
{
	serial_t *h = calloc(sizeof(serial_t), 1);

	COMMTIMEOUTS timeouts = {MAXDWORD, MAXDWORD, 3000, 0, 0}; //read timeout 3sec

	/* Fix the device name if required */
	char *devName;
	if (strlen(device) > 4 && device[0] != '\\') {
		devName = calloc(1, strlen(device) + 5);
		sprintf(devName, "\\\\.\\%s", device);
	} else {
		devName = (char *)device;
	}

	/* Create file handle for port */
	h->fd = CreateFile(devName, GENERIC_READ | GENERIC_WRITE, 
			0, /* Exclusive access */
			NULL, /* No security */
			OPEN_EXISTING,
			0, //FILE_FLAG_OVERLAPPED,
			NULL);

	if (devName != device)
		free(devName);
	
	if (h->fd == INVALID_HANDLE_VALUE) {
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
			fprintf(stderr, "File not found: %s\n", device);
		return NULL;
	}

	SetupComm(h->fd, 4096, 4096); /* Set input and output buffer size */

	SetCommTimeouts(h->fd, &timeouts);

	SetCommMask(h->fd, EV_ERR); /* Notify us of error events */

	GetCommState(h->fd, &h->oldtio); /* Retrieve port parameters */
	GetCommState(h->fd, &h->newtio); /* Retrieve port parameters */

	//PurgeComm(h->fd, PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

	return h;
}

void serial_close(serial_t *h) 
{
	if(!h || h->fd == INVALID_HANDLE_VALUE)
		return;

	serial_flush(h);
	SetCommState(h->fd, &h->oldtio);
	CloseHandle(h->fd);
	free(h);
}

void serial_flush(const serial_t *h) 
{
	if(!h || h->fd == INVALID_HANDLE_VALUE)
		return;
	PurgeComm(h->fd, PURGE_TXCLEAR|PURGE_RXCLEAR);
}

serial_err_t serial_setup(serial_t *h, 
			  const serial_baud_t baud, 
			  const serial_bits_t bits, 
			  const serial_parity_t parity, 
			  const serial_stopbit_t stopbit) 
{
	if(!h || h->fd == INVALID_HANDLE_VALUE)
		return SERIAL_ERR_NOT_CONFIGURED;

	switch(baud) {
		case SERIAL_BAUD_110   : h->newtio.BaudRate = CBR_110   ; break;
		case SERIAL_BAUD_300   : h->newtio.BaudRate = CBR_300   ; break;
		case SERIAL_BAUD_600   : h->newtio.BaudRate = CBR_600   ; break;
		case SERIAL_BAUD_1200  : h->newtio.BaudRate = CBR_1200  ; break;
		case SERIAL_BAUD_2400  : h->newtio.BaudRate = CBR_2400  ; break;
		case SERIAL_BAUD_4800  : h->newtio.BaudRate = CBR_4800  ; break;
		case SERIAL_BAUD_9600  : h->newtio.BaudRate = CBR_9600  ; break;
		case SERIAL_BAUD_14400 : h->newtio.BaudRate = CBR_14400 ; break;
		case SERIAL_BAUD_19200 : h->newtio.BaudRate = CBR_19200 ; break;
		case SERIAL_BAUD_38400 : h->newtio.BaudRate = CBR_38400 ; break;
		case SERIAL_BAUD_56000 : h->newtio.BaudRate = CBR_56000 ; break;
		case SERIAL_BAUD_57600 : h->newtio.BaudRate = CBR_57600 ; break;
		case SERIAL_BAUD_115200: h->newtio.BaudRate = CBR_115200; break;
		case SERIAL_BAUD_128000: h->newtio.BaudRate = CBR_128000; break;
		case SERIAL_BAUD_256000: h->newtio.BaudRate = CBR_256000; break;

		case SERIAL_BAUD_INVALID: return SERIAL_ERR_INVALID_BAUD; break;
		default:
		/* These are not defined in WinBase.h and might work or not */
			h->newtio.BaudRate = serial_get_baud_int(baud);
	}

	switch(bits) {
		case SERIAL_BITS_5: h->newtio.ByteSize = 5; break;
		case SERIAL_BITS_6: h->newtio.ByteSize = 6; break;
		case SERIAL_BITS_7: h->newtio.ByteSize = 7; break;
		case SERIAL_BITS_8: h->newtio.ByteSize = 8; break;

		default:
			return SERIAL_ERR_INVALID_BITS;
	}

	switch(parity) {
		case SERIAL_PARITY_NONE: h->newtio.Parity = NOPARITY;   break;
		case SERIAL_PARITY_EVEN: h->newtio.Parity = EVENPARITY; break;
		case SERIAL_PARITY_ODD : h->newtio.Parity = ODDPARITY;  break;

		default:
			return SERIAL_ERR_INVALID_PARITY;
	}

	switch(stopbit) {
		case SERIAL_STOPBIT_1: h->newtio.StopBits = ONESTOPBIT;	 break;
		case SERIAL_STOPBIT_2: h->newtio.StopBits = TWOSTOPBITS; break;

		default:
			return SERIAL_ERR_INVALID_STOPBIT;
	}

	/* if the port is already configured, no need to do anything */
	if (
		h->configured        &&
		h->baud	   == baud   &&
		h->bits	   == bits   &&
		h->parity  == parity &&
		h->stopbit == stopbit
	) return SERIAL_ERR_OK;

	/* reset the settings */
	h->newtio.fOutxCtsFlow = FALSE;
	h->newtio.fOutxDsrFlow = FALSE;
	h->newtio.fOutX = FALSE;
	h->newtio.fInX = FALSE;
	h->newtio.fNull = 0;
	h->newtio.fAbortOnError = 0;

	/* set the settings */
	serial_flush(h);
	if (!SetCommState(h->fd, &h->newtio))
		return SERIAL_ERR_SYSTEM;

	h->configured = 1;
	h->baud	      = baud;
	h->bits	      = bits;
	h->parity     = parity;
	h->stopbit    = stopbit;
	return SERIAL_ERR_OK;
}

serial_err_t serial_write(const serial_t *h, const void *buffer, unsigned int len) 
{
	if(!h || h->fd == INVALID_HANDLE_VALUE || !h->configured)
		return SERIAL_ERR_NOT_CONFIGURED;
	if(!buffer || !len)
		return SERIAL_ERR_WRONG_ARG;

	DWORD r;
	uint8_t *pos = (uint8_t*)buffer;

	while(len > 0) {
		if(!WriteFile(h->fd, pos, len, &r, NULL))
			return SERIAL_ERR_SYSTEM;
		if (r < 1) return SERIAL_ERR_SYSTEM;

		len -= r;
		pos += r;
	}

	return SERIAL_ERR_OK;
}

serial_err_t serial_read(const serial_t *h, const void *buffer, unsigned int len, unsigned int *readed)
{
	if(!h || h->fd == INVALID_HANDLE_VALUE || !h->configured)
		return SERIAL_ERR_NOT_CONFIGURED;
	if(!buffer || !len)
		return SERIAL_ERR_WRONG_ARG;

	DWORD r;
	uint8_t *pos = (uint8_t*)buffer;

	while(len > 0) {
		ReadFile(h->fd, pos, len, &r, NULL);
		      if (r == 0) return SERIAL_ERR_NODATA;
		else  if (r <  0) return SERIAL_ERR_SYSTEM;

		len -= r;
		pos += r;
		if(readed) *readed += r;
	}

	return SERIAL_ERR_OK;
}

const char* serial_get_setup_str(const serial_t *h) 
{
	static char str[11];
	if (!h || !h->configured)
		snprintf(str, sizeof(str), "INVALID");
	else
		snprintf(str, sizeof(str), "%u %d%c%d",
			serial_get_baud_int   (h->baud   ),
			serial_get_bits_int   (h->bits   ),
			serial_get_parity_str (h->parity ),
			serial_get_stopbit_int(h->stopbit)
		);

	return str;
}
