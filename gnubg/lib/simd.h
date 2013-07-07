/* $Id$ 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SIMD_H
#define SIMD_H

#if USE_SIMD_INSTRUCTIONS

#include <stdlib.h>

#ifdef USE_AVX
#define ALIGN_SIZE 32
#define VEC_SIZE 8
#define LOG2VEC_SIZE 3
#define float_vector __m256
#define int_vector __m256i
#else
#define ALIGN_SIZE 16
#define VEC_SIZE 4
#define LOG2VEC_SIZE 2
#define float_vector __m128
#define int_vector __m128i
#endif

#ifdef _MSC_VER
#define SSE_ALIGN(D) __declspec(align(ALIGN_SIZE)) D
#else
#define SSE_ALIGN(D) D __attribute__ ((aligned(ALIGN_SIZE)))
#endif

#define sse_aligned(ar) (!(((int)ar) % ALIGN_SIZE))

extern float *sse_malloc(size_t size);
extern void sse_free(float *ptr);

#else

#define SSE_ALIGN(D) D
#define sse_malloc malloc
#define sse_free free

#endif

#endif /* SIMD_H */
