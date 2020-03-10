/*-
 * Copyright (c) 2012 NetApp, Inc.
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

#include <types.h>
#include <vm.h>
#include <vuart.h>

/**
 * @addtogroup debug
 *
 * @{
 */

/**
 * @file
 * @brief This file declares the functions related to virtual UART operations, including initialization and
 * deinitialization.
 *
 * This file is decomposed into the following functions:
 *
 * - init_vuart(vm, vu_config)   Initialize the hypervisor virtual UART which is solely for debugging. No operation in
 *                               release version.
 * - deinit_vuart(vm)            Deinitialize the hypervisor virtual UART which is solely for debugging. No operation
 *                               in release version.
 */

/**
 * @brief Initialize the hypervisor virtual UART which is solely for debugging. No operation in release version.
 *
 * @param[inout]    vm        A pointer to the VM whose virtual UART data structure will be initialized. It will be
 *                            unused in release version.
 * @param[in]       vu_config A pointer to virtual UART configuration data structure which is used to configure the
 *                            vuart.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_INIT
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void init_vuart(__unused struct acrn_vm *vm, __unused const struct vuart_config *vu_config)
{
}

/**
 * @brief Deinitialize the hypervisor virtual UART which is solely for debugging. No operation in release version.
 *
 * @param[inout]    vm A pointer to the VM whose virtual UART data structure will be deinitialized. It will be unused in
 *                     release version.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @mode HV_TERMINATION
 *
 * @reentrancy Unspecified
 * @threadsafety Unspecified
 */
void deinit_vuart(__unused struct acrn_vm *vm)
{
}

/**
 * @}
 */
