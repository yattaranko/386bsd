/*-
 * Copyright (c) 1980, 1983, 1990 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)qsort.c	5.9 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

#if 0
#include <sys/types.h>
#include <stdlib.h>

/*
 * MTHRESH is the smallest partition for which we compare for a median
 * value instead of using the middle value.
 */
#define	MTHRESH	6

/*
 * THRESH is the minimum number of entries in a partition for continued
 * partitioning.
 */
#define	THRESH	4

static void insertion_sort(char *bot, register int size, int nmemb, (*compar)());
static void quick_sort(register char* bot, int nmemb, register int size, int (*compar)());

void
qsort(bot, nmemb, size, compar)
	void *bot;
	size_t nmemb, size;
	int (*compar) __P((const void *, const void *));
{
/*	static void insertion_sort(), quick_sort(); */

	if (nmemb <= 1)
		return;

	if (nmemb >= THRESH)
		quick_sort(bot, nmemb, size, compar);
	else
		insertion_sort(bot, nmemb, size, compar);
}

/*
 * Swap two areas of size number of bytes.  Although qsort(3) permits random
 * blocks of memory to be sorted, sorting pointers is almost certainly the
 * common case (and, were it not, could easily be made so).  Regardless, it
 * isn't worth optimizing; the SWAP's get sped up by the cache, and pointer
 * arithmetic gets lost in the time required for comparison function calls.
 */
#define	SWAP(a, b) { \
	cnt = size; \
	do { \
		ch = *a; \
		*a++ = *b; \
		*b++ = ch; \
	} while (--cnt); \
}

/*
 * Knuth, Vol. 3, page 116, Algorithm Q, step b, argues that a single pass
 * of straight insertion sort after partitioning is complete is better than
 * sorting each small partition as it is created.  This isn't correct in this
 * implementation because comparisons require at least one (and often two)
 * function calls and are likely to be the dominating expense of the sort.
 * Doing a final insertion sort does more comparisons than are necessary
 * because it compares the "edges" and medians of the partitions which are
 * known to be already sorted.
 *
 * This is also the reasoning behind selecting a small THRESH value (see
 * Knuth, page 122, equation 26), since the quicksort algorithm does less
 * comparisons than the insertion sort.
 */
#define	SORT(bot, n) { \
	if (n > 1) \
		if (n == 2) { \
			t1 = bot + size; \
			if (compar(t1, bot) < 0) \
				SWAP(t1, bot); \
		} else \
			insertion_sort(bot, n, size, compar); \
}

static void
quick_sort(register char* bot, int nmemb, register int size, int (*compar)())
{
	register int cnt;
	register u_char ch;
	register char *top, *mid, *t1, *t2;
	register int n1, n2;
	char *bsv;
/*	static void insertion_sort(); */

	/* bot and nmemb must already be set. */
partition:

	/* find mid and top elements */
	mid = bot + size * (nmemb >> 1);
	top = bot + (nmemb - 1) * size;

	/*
	 * Find the median of the first, last and middle element (see Knuth,
	 * Vol. 3, page 123, Eq. 28).  This test order gets the equalities
	 * right.
	 */
	if (nmemb >= MTHRESH) {
		n1 = compar(bot, mid);
		n2 = compar(mid, top);
		if (n1 < 0 && n2 > 0)
			t1 = compar(bot, top) < 0 ? top : bot;
		else if (n1 > 0 && n2 < 0)
			t1 = compar(bot, top) > 0 ? top : bot;
		else
			t1 = mid;

		/* if mid element not selected, swap selection there */
		if (t1 != mid) {
			SWAP(t1, mid);
			mid -= size;
		}
	}

	/* Standard quicksort, Knuth, Vol. 3, page 116, Algorithm Q. */
#define	didswap	n1
#define	newbot	t1
#define	replace	t2
	didswap = 0;
	for (bsv = bot;;) {
		for (; bot < mid && compar(bot, mid) <= 0; bot += size);
		while (top > mid) {
			if (compar(mid, top) <= 0) {
				top -= size;
				continue;
			}
			newbot = bot + size;	/* value of bot after swap */
			if (bot == mid)		/* top <-> mid, mid == top */
				replace = mid = top;
			else {			/* bot <-> top */
				replace = top;
				top -= size;
			}
			goto swap;
		}
		if (bot == mid)
			break;

		/* bot <-> mid, mid == bot */
		replace = mid;
		newbot = mid = bot;		/* value of bot after swap */
		top -= size;

swap:		SWAP(bot, replace);
		bot = newbot;
		didswap = 1;
	}

	/*
	 * Quicksort behaves badly in the presence of data which is already
	 * sorted (see Knuth, Vol. 3, page 119) going from O N lg N to O N^2.
	 * To avoid this worst case behavior, if a re-partitioning occurs
	 * without swapping any elements, it is not further partitioned and
	 * is insert sorted.  This wins big with almost sorted data sets and
	 * only loses if the data set is very strangely partitioned.  A fix
	 * for those data sets would be to return prematurely if the insertion
	 * sort routine is forced to make an excessive number of swaps, and
	 * continue the partitioning.
	 */
	if (!didswap) {
		insertion_sort(bsv, nmemb, size, compar);
		return;
	}

	/*
	 * Re-partition or sort as necessary.  Note that the mid element
	 * itself is correctly positioned and can be ignored.
	 */
#define	nlower	n1
#define	nupper	n2
	bot = bsv;
	nlower = (mid - bot) / size;	/* size of lower partition */
	mid += size;
	nupper = nmemb - nlower - 1;	/* size of upper partition */

	/*
	 * If must call recursively, do it on the smaller partition; this
	 * bounds the stack to lg N entries.
	 */
	if (nlower > nupper) {
		if (nupper >= THRESH)
			quick_sort(mid, nupper, size, compar);
		else {
			SORT(mid, nupper);
			if (nlower < THRESH) {
				SORT(bot, nlower);
				return;
			}
		}
		nmemb = nlower;
	} else {
		if (nlower >= THRESH)
			quick_sort(bot, nlower, size, compar);
		else {
			SORT(bot, nlower);
			if (nupper < THRESH) {
				SORT(mid, nupper);
				return;
			}
		}
		bot = mid;
		nmemb = nupper;
	}
	goto partition;
	/* NOTREACHED */
}

static void
insertion_sort(char *bot, register int size, int nmemb, (*compar)())
{
	register int cnt;
	register u_char ch;
	register char *s1, *s2, *t1, *t2, *top;

	/*
	 * A simple insertion sort (see Knuth, Vol. 3, page 81, Algorithm
	 * S).  Insertion sort has the same worst case as most simple sorts
	 * (O N^2).  It gets used here because it is (O N) in the case of
	 * sorted data.
	 */
	top = bot + nmemb * size;
	for (t1 = bot + size; t1 < top;) {
		for (t2 = t1; (t2 -= size) >= bot && compar(t1, t2) < 0;);
		if (t1 != (t2 += size)) {
			/* Bubble bytes up through each element. */
			for (cnt = size; cnt--; ++t1) {
				ch = *t1;
				for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
					*s1 = *s2;
				*s1 = ch;
			}
		} else
			t1 += size;
	}
}
#else
#include <sys/param.h>

#ifdef I_AM_QSORT_R
typedef int		 cmp_t(const void *, const void *, void *);
#else
typedef int		 cmp_t(const void *, const void *);
#endif
static inline char	*med3(char *, char *, char *, cmp_t *, void *);
static inline void	 swapfunc(char *, char *, size_t, int, int);

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define	swapcode(TYPE, parmi, parmj, n) {		\
	size_t i = (n) / sizeof (TYPE);			\
	TYPE *pi = (TYPE *) (parmi);		\
	TYPE *pj = (TYPE *) (parmj);		\
	do { 						\
		TYPE	t = *pi;		\
		*pi++ = *pj;				\
		*pj++ = t;				\
	} while (--i > 0);				\
}

#define	SWAPINIT(TYPE, a, es) swaptype_ ## TYPE =	\
	((char *)a - (char *)0) % sizeof(TYPE) ||	\
	es % sizeof(TYPE) ? 2 : es == sizeof(TYPE) ? 0 : 1;

static inline void
swapfunc(char *a, char *b, size_t n, int swaptype_long, int swaptype_int)
{
	if (swaptype_long <= 1)
		swapcode(long, a, b, n)
	else if (swaptype_int <= 1)
		swapcode(int, a, b, n)
	else
		swapcode(char, a, b, n)
}

#define	swap(a, b)					\
	if (swaptype_long == 0) {			\
		long t = *(long *)(a);			\
		*(long *)(a) = *(long *)(b);		\
		*(long *)(b) = t;			\
	} else if (swaptype_int == 0) {			\
		int t = *(int *)(a);			\
		*(int *)(a) = *(int *)(b);		\
		*(int *)(b) = t;			\
	} else						\
		swapfunc(a, b, es, swaptype_long, swaptype_int)

#define	vecswap(a, b, n)				\
	if ((n) > 0) swapfunc(a, b, n, swaptype_long, swaptype_int)

#ifdef I_AM_QSORT_R
#define	CMP(t, x, y) (cmp((x), (y), (t)))
#else
#define	CMP(t, x, y) (cmp((x), (y)))
#endif

static inline char *
med3(char *a, char *b, char *c, cmp_t *cmp, void *thunk)
{
	return CMP(thunk, a, b) < 0 ?
	       (CMP(thunk, b, c) < 0 ? b : (CMP(thunk, a, c) < 0 ? c : a ))
	      :(CMP(thunk, b, c) > 0 ? b : (CMP(thunk, a, c) < 0 ? a : c ));
}

#ifdef I_AM_QSORT_R
void
(qsort_r)(void *a, size_t n, size_t es, cmp_t *cmp, void *thunk)
#else
#define	thunk NULL
void
qsort(void *a, size_t n, size_t es, cmp_t *cmp)
#endif
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	size_t d1, d2;
	int cmp_result;
	int swaptype_long, swaptype_int, swap_cnt;

loop:	SWAPINIT(long, a, es);
	SWAPINIT(int, a, es);
	swap_cnt = 0;
	if (n < 7) {
		for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es)
			for (pl = pm; 
			     pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}
	pm = (char *)a + (n / 2) * es;
	if (n > 7) {
		pl = a;
		pn = (char *)a + (n - 1) * es;
		if (n > 40) {
			size_t d = (n / 8) * es;

			pl = med3(pl, pl + d, pl + 2 * d, cmp, thunk);
			pm = med3(pm - d, pm, pm + d, cmp, thunk);
			pn = med3(pn - 2 * d, pn - d, pn, cmp, thunk);
		}
		pm = med3(pl, pm, pn, cmp, thunk);
	}
	swap(a, pm);
	pa = pb = (char *)a + es;

	pc = pd = (char *)a + (n - 1) * es;
	for (;;) {
		while (pb <= pc && (cmp_result = CMP(thunk, pb, a)) <= 0) {
			if (cmp_result == 0) {
				swap_cnt = 1;
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (cmp_result = CMP(thunk, pc, a)) >= 0) {
			if (cmp_result == 0) {
				swap_cnt = 1;
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
		}
		if (pb > pc)
			break;
		swap(pb, pc);
		swap_cnt = 1;
		pb += es;
		pc -= es;
	}
	if (swap_cnt == 0) {  /* Switch to insertion sort */
		for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es)
			for (pl = pm; 
			     pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}

	pn = (char *)a + n * es;
	d1 = MIN(pa - (char *)a, pb - pa);
	vecswap(a, pb - d1, d1);
	d1 = MIN(pd - pc, pn - pd - es);
	vecswap(pb, pn - d1, d1);

	d1 = pb - pa;
	d2 = pd - pc;
	if (d1 <= d2) {
		/* Recurse on left partition, then iterate on right partition */
		if (d1 > es) {
#ifdef I_AM_QSORT_R
			qsort_r(a, d1 / es, es, cmp, thunk);
#else
			qsort(a, d1 / es, es, cmp);
#endif
		}
		if (d2 > es) {
			/* Iterate rather than recurse to save stack space */
			/* qsort(pn - d2, d2 / es, es, cmp); */
			a = pn - d2;
			n = d2 / es;
			goto loop;
		}
	} else {
		/* Recurse on right partition, then iterate on left partition */
		if (d2 > es) {
#ifdef I_AM_QSORT_R
			qsort_r(pn - d2, d2 / es, es, cmp, thunk);
#else
			qsort(pn - d2, d2 / es, es, cmp);
#endif
		}
		if (d1 > es) {
			/* Iterate rather than recurse to save stack space */
			/* qsort(a, d1 / es, es, cmp); */
			n = d1 / es;
			goto loop;
		}
	}
}
#endif