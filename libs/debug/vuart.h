/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef DEBUG_VUART_H
#define DEBUG_VUART_H

#include <vuart.h>
#include <types.h>
#include <spinlock.h>
#include <vm_config.h>

#define RX_BUF_SIZE		256U
#define TX_BUF_SIZE		65536U
#define INVAILD_VUART_IDX	0xFFU

#define INVALID_COM_BASE	0U

struct vuart_fifo {
	char *buf;
	uint32_t rindex;	/* index to read from */
	uint32_t windex;	/* index to write to */
	uint32_t num;		/* number of characters in the fifo */
	uint32_t size;		/* size of the fifo */
};

struct acrn_vuart {
	uint8_t data;		/* Data register (R/W) */
	uint8_t ier;		/* Interrupt enable register (R/W) */
	uint8_t lcr;		/* Line control register (R/W) */
	uint8_t mcr;		/* Modem control register (R/W) */
	uint8_t lsr;		/* Line status register (R/W) */
	uint8_t msr;		/* Modem status register (R/W) */
	uint8_t fcr;		/* FIFO control register (W) */
	uint8_t scr;		/* Scratch register (R/W) */
	uint8_t dll;		/* Baudrate divisor latch LSB */
	uint8_t dlh;		/* Baudrate divisor latch MSB */

	struct vuart_fifo rxfifo;
	struct vuart_fifo txfifo;
	uint16_t port_base;
	uint32_t irq;
	char vuart_rx_buf[RX_BUF_SIZE];
	char vuart_tx_buf[TX_BUF_SIZE];
	bool thre_int_pending;	/* THRE interrupt pending */
	bool active;
	struct acrn_vuart *target_vu; /* Pointer to target vuart */
	struct acrn_vm *vm;
	spinlock_t lock;	/* protects all softc elements */
};

void vuart_putchar(struct acrn_vuart *vu, char ch);
char vuart_getchar(struct acrn_vuart *vu);

bool is_vuart_intx(struct acrn_vm *vm, uint32_t intx_pin);

struct acrn_vuart *vm_console_vuart(struct acrn_vm *vm);

#endif /* DEBUG_VUART_H */
