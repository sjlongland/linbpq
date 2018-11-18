#ifndef _ROUNDPTR_H
#define _ROUNDPTR_H
/*
 * Copyright 2001-2018 John Wiseman G8BPQ
 * Original Credit: Stuart Longland VK4MSL
 *
 * This file is part of LinBPQ/BPQ32.
 *
 * LinBPQ/BPQ32 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LinBPQ/BPQ32 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LinBPQ/BPQ32.  If not, see http://www.gnu.org/licenses
 */

#include <stdint.h>

/*!
 * Round a pointer up to the next word boundary.  This ensures that the
 * pointer returned meets the native word alignment requirements of the
 * processor architecture.
 */
static inline void * roundPtr(void * ptr) {
	intptr_t ptrAddr = (intptr_t)ptr;

	/* Advance it the width of a pointer, minus one byte */
	ptrAddr += (sizeof(intptr_t) - 1);

	/* Mask off the least significant bits */
	ptrAddr &= ~((intptr_t)(sizeof(intptr_t) - 1));

	return (void*)ptrAddr;
}

#endif
