/**
 * Copyright (c) 2013 by The George Washington University, Jakob Kaivo
 * and Gabe Parmer.
 * 
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * Initial Author: Jakob Kaivo, jkaivo@gwu.edu, 2013.
 * Additional: Gabe Parmer, gparmer@gwu.edu, 2013.
 */

#include "include/tcap.h"
#include <limits.h>

/* Fill in default "safe" values */
static void
tcap_init(struct tcap *t, struct spd *c)
{
	t->ndelegs              = 1;
	t->epoch                = 0;
	t->budget_local.cycles  = 0LL;
	t->cpuid                = get_cpuid();
	t->delegations[0].prio  = TCAP_PRIO_MAX;
	t->delegations[0].sched = c;
	t->freelist             = NULL;
	t->sched_info           = 0;
	tcap_ref_create(&t->budget, t);
}

static int 
tcap_delete(struct spd *s, struct tcap *tcap)
{
	assert(s && tcap);
	assert(tcap < &s->tcaps[TCAP_MAX] && tcap >= &s->tcaps[0]);
	/* Can't delete your persistent tcap! */
	if (&s->tcaps[0] == tcap) return -1;
	tcap->epoch++; /* now all existing references to the tcap are invalid */
	tcap->freelist   = s->tcap_freelist;
	s->tcap_freelist = tcap;
	memset(&tcap->budget_local, 0, sizeof(struct tcap_budget));
	memset(tcap->delegations, 0, sizeof(struct tcap_sched_info) * TCAP_MAX_DELEGATIONS);
	tcap->ndelegs    = tcap->cpuid = 0;
	tcap->sched_info = 0;
	s->ntcaps--;

	return 0;
}

void 
tcap_spd_init(struct spd *c)
{
	int i;
	struct tcap *t;

	c->tcap_freelist = &c->tcaps[1];
	c->ntcaps        = 1;
	for (i = 1 ; i < TCAP_MAX ; i++) {
		t                         = &c->tcaps[i];
		t->ndelegs = t->epoch     = t->cpuid = 0;
		t->freelist               = &c->tcaps[i+1];
	}
	c->tcaps[TCAP_MAX-1].freelist = NULL;

	/* initialize tcap */
	t = &c->tcaps[0];
	tcap_init(t, c);
	t->budget_local.cycles = TCAP_CYC_INF;
}

int
tcap_spd_delete(struct spd *spd)
{
	int i;
	
	assert(spd);
	for (i = 0 ; i < TCAP_MAX ; i++) {
		if (!tcap_is_allocated(&spd->tcaps[i])) continue;
		tcap_delete(spd, &(spd->tcaps[i]));
	}
	spd->ntcaps = 0;

	return 0;
}

tcap_t
tcap_id(struct tcap *t)
{
	assert(t && tcap_is_allocated(t));
	return t - tcap_sched_info(t)->sched->tcaps;
}

struct tcap *
tcap_get(struct spd *c, tcap_t id)
{
	struct tcap *t;

	assert(c);
	if (unlikely(id >= TCAP_MAX)) return NULL;
	t = &c->tcaps[id];
	if (unlikely(!tcap_is_allocated(t))) return NULL;
	return t;
}

/* 
 * Set thread t to be bound to tcap.  Its execution will proceed with
 * that tcap from this point on.  This is most useful for interrupt
 * threads.
 */
int 
tcap_bind(struct thread *t, struct tcap *tcap)
{
	assert(t && tcap && tcap_sched_info(tcap)->sched);
	if (!thd_scheduled_by(t, tcap_sched_info(tcap)->sched)) return -1;
	tcap_ref_create(&t->tcap_active, tcap);
	return 0;
}

/* 
 * This all makes the assumption that the first entry in the delegate
 * array for the tcap is the root capability (the fountain of time).
 */
int
__tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, 
		s32_t __cycles, u16_t prio, int pooled)
{
	struct tcap_budget *b;
	struct tcap *bc;
	s64_t cycles = __cycles;

	assert(tcapdst && tcapsrc);
	if (unlikely(tcapsrc->cpuid != get_cpuid() || 
		     tcapdst->cpuid != tcapsrc->cpuid || cycles < 0)) return -1;
	bc = tcap_deref(&tcapsrc->budget);
	if (unlikely(!bc)) return -1;
	b = &bc->budget_local;

	if (!prio) prio = tcap_sched_info(tcapsrc)->prio;

	/* update the source tcap */
	if (!pooled) {
		if (!cycles) cycles = b->cycles;
		if (unlikely(b->cycles < 0LL || b->cycles < cycles)) return -1;

		tcap_ref_create(&tcapdst->budget, tcapdst);
		if (tcapdst->budget_local.cycles != TCAP_CYC_INF) {
			tcapdst->budget_local.cycles += cycles;
		}
		if (b->cycles != TCAP_CYC_INF) {
			b->cycles -= cycles;
		}
	} else {
		/* inherit the (possibly inherited) budget */
		memcpy(&tcapdst->budget, &tcapsrc->budget, sizeof(struct tcap_ref));
	}
	tcap_sched_info(tcapdst)->prio = prio;

	return 0;
}

/* 
 * pooled = 1 -> share the budget with t.  Otherwise:
 * cycles = 0 means remove all cycles from existing tcap
 * 
 * prio = 0 denotes inheriting the priority (lower values = higher priority)
 *
 * Error conditions include t->cycles < cycles, prio < t->prio
 * (ignoring values of 0).
 */
struct tcap *
tcap_split(struct tcap *t, s32_t cycles, u16_t prio, int pooled)
{
	struct tcap *n;
	struct spd  *c;

	assert(t);
	if (t->cpuid != get_cpuid()) return NULL;
	c = tcap_sched_info(t)->sched;
	assert(c);
	n                = c->tcap_freelist;
	if (unlikely(!n)) return NULL;
	c->tcap_freelist = n->freelist;

	tcap_init(n, c);
	c->ntcaps++;
	n->ndelegs       = t->ndelegs;
	n->sched_info    = t->sched_info;
	memcpy(n->delegations, t->delegations, 
	       sizeof(struct tcap_sched_info) * t->ndelegs);

	if (__tcap_transfer(n, t, cycles, prio, pooled)) {
		tcap_delete(c, n);
		n = NULL;
	}

	return n;
}

/* 
 * Which tcap should receive delegations while executing in thread t?
 */
int 
tcap_receiver(struct thread *t, struct tcap *tcap)
{
	assert(t && tcap);
	if (!thd_scheduled_by(t, tcap_sched_info(tcap)->sched)) return -1;
	tcap_ref_create(&t->tcap_receiver, tcap);
	return 0;
}

/* 
 * Do the delegation chains for the destination and the source tcaps
 * enable time to be transferred from the source to the destination?
 *
 * Specifically, can we do the delegation given the delegation history
 * of both of the tcaps?  If the source has been delegated to by
 * schedulers that the destination has not, then we would be moving
 * time from a more restricted environment to a less restricted one.
 * Not OK as this will leak time in a manner that the parent could not
 * control.  If any of the source delegations have a lower numerical
 * priority in the destination, we would be leaking time to a
 * higher-priority part of the system, thus heightening the status of
 * that time.  
 *
 * Put simply, this checks:
 * dst \subset src \wedge 
 * \forall_{s \in dst, s != src[src_sched]} sched.prio <= 
 *                                          src[sched.id].prio
 */
static int
__tcap_legal_transfer_delegs(struct tcap_sched_info *dst_ds, int d_nds, struct spd *dsched, 
			     struct tcap_sched_info *src_ds, int s_nds, struct spd *ssched)
{
	int i, j;

	for (i = 0, j = 0 ; i < d_nds && j < s_nds ; ) {
		struct tcap_sched_info *s, *d;

		d = &dst_ds[i];
		s = &src_ds[j];

		/* 
		 * Ignore the current scheduler; it is allowed to
		 * change its own tcap's priorities, and is not in its
		 * own delegation list.
		 */
		if (d->sched == ssched) { 
			i++;
			continue;
		}
		if (d->sched == s->sched) {
			if (d->prio < s->prio) return -1;
			/* another option is to _degrade_ the
			 * destination by manually lower the
			 * delegation's priority.  However, I think
			 * having a more predictable check is more
			 * important, rather than perhaps causing
			 * transparent degradation of priority. */
			i++;
		}
		/* OK so far, look at the next comparison */
		j++;
	}
	if (j == s_nds && i != d_nds) return -1;

	return 0;
}

static inline int 
__tcap_legal_transfer(struct tcap *dst, struct tcap *src)
{
	return __tcap_legal_transfer_delegs(dst->delegations, dst->ndelegs, tcap_sched_info(dst)->sched, 
					    src->delegations, src->ndelegs, tcap_sched_info(src)->sched);
}

int
tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, 
	      s32_t cycles, u16_t prio, int pooled) 
{
	if (__tcap_legal_transfer(tcapdst, tcapsrc)) return -1;
	return __tcap_transfer(tcapdst, tcapsrc, cycles, prio, pooled);
}

int
tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc, 
	      int cycles, int prio, int pooled)
{
	struct tcap_sched_info *td;
	struct tcap_sched_info deleg_tmp[TCAP_MAX_DELEGATIONS];
	int ndelegs, i, j;
	struct spd *d;

	assert(tcapdst && tcapsrc);
	if (unlikely(tcapdst->ndelegs >= TCAP_MAX_DELEGATIONS)) {
		printk("tcap %x already has max number of delgations.\n", 
			tcap_id(tcapdst));
		return -1;
	}
	d = tcap_sched_info(tcapdst)->sched;
	if (d == tcap_sched_info(tcapsrc)->sched) return -1;
	if (!prio) prio = tcap_sched_info(tcapsrc)->prio;

	for (i = 0, j = 0, ndelegs = 0 ; 
	     i < tcapdst->ndelegs && j < tcapsrc->ndelegs ; ndelegs++) {
		struct tcap_sched_info *s, t;

		if (tcapdst->delegations[i].sched < tcapsrc->delegations[j].sched) {
			s = &tcapdst->delegations[i++];
		} else if (tcapdst->delegations[i].sched > tcapsrc->delegations[j].sched) {
			s = &tcapsrc->delegations[j++];
		} else {	/* same scheduler */
			memcpy(&t, &tcapsrc->delegations[j], sizeof(struct tcap_sched_info));
			if (tcapdst->delegations[i].prio < tcapsrc->delegations[j].prio) {
				t.prio = tcapsrc->delegations[j].prio;
			} else {
				t.prio = tcapdst->delegations[j].prio;
			}
			s = &t;
			i++;
			j++;
		}
		if (s->sched == tcap_sched_info(tcapsrc)->sched) s->prio = prio;
		if (ndelegs == TCAP_MAX_DELEGATIONS) return -1;
		memcpy(&deleg_tmp[ndelegs], s, sizeof(struct tcap_sched_info));
	}

	return 0;

 	/* 
	 * Constrain the destination capability identically to the
	 * source capability.  Remember, given the above check, we
	 * know that the destination capability has a subset of the
	 * delegations of the source.
	 */
	ndelegs = tcapsrc->ndelegs;
	memcpy(deleg_tmp, tcapsrc->delegations, 
	       sizeof(struct tcap_sched_info) * ndelegs);

	if (!prio) prio = tcap_sched_info(tcapsrc)->prio;
	/* adjust current scheduler's priority */
	for (i = 0 ; i < ndelegs && prio ; i++) {
		if (deleg_tmp[i].sched == tcap_sched_info(tcapsrc)->sched) {
			deleg_tmp[i].prio = prio;
			break;
		}
	}
	assert(i != ndelegs); 	/* must have source scheduler */
	/* 
	 * Add the delegated-to scheduler, and maintain a
	 * scheduler-address ordering in the delegation array for O(N)
	 * comparison in tcap_higher_prio.
	 */
	td = NULL;
	for (i = 0 ; i < ndelegs ; i++) {
		if (deleg_tmp[i].sched == d) td = &deleg_tmp[i];
		if (deleg_tmp[i].sched > d)  break;
	}
//	printf("td %p @ %d, ndelegs %d\n", td, i, ndelegs);
	/* we know we need to add the scheduler at position "i" */
	if (!td && i == TCAP_MAX_DELEGATIONS) return -1; /* no room! */
	/* add entry by shifting all entries up one */
	if (!td) {
		td = &deleg_tmp[i];
//		printf("%d, %d\n", i, ndelegs);
		for (j = ndelegs ; j > i ; j--) {
//			printf("memcpy from %p:%d <- %p:%d\n", 
//			       deleg_tmp[j].sched, deleg_tmp[j].prio, 
//			       deleg_tmp[j-1].sched, deleg_tmp[j-1].prio);
			memcpy(&deleg_tmp[j], &deleg_tmp[j-1], 
			       sizeof(struct tcap_sched_info));
		}
		ndelegs++;
//		printf("%d: s %p, p %d -> %p:%d\n", 
//		       i, deleg_tmp[i].sched, deleg_tmp[i].prio,
//		       d, tcap_sched_info(tcapdst)->prio);
	} 
	/* ...and add/update our destination scheduler's entry */
	td->sched = d;
	td->prio  = tcap_sched_info(tcapdst)->prio;

	/* printf("dst %p\n", tcap_sched_info(tcapdst)->sched); */
	/* for (i = 0 ; i < tcapdst->ndelegs ; i++) printf("\t%p:%d\n", tcapdst->delegations[i].sched, tcapdst->delegations[i].prio); */
	/* printf("src %p\n", tcap_sched_info(tcapsrc)->sched); */
	/* for (i = 0 ; i < ndelegs ; i++) printf("\t%p:%d\n", deleg_tmp[i].sched, deleg_tmp[i].prio); */


	if (__tcap_legal_transfer_delegs(tcapdst->delegations, tcapdst->ndelegs, tcap_sched_info(tcapdst)->sched, 
					 deleg_tmp, ndelegs, tcap_sched_info(tcapsrc)->sched)) return -1;

//	printf("<<>>\n");

	/* 
	 * Adjust priorities in the new array of delegations such that
	 * each entry in the destination array (from the source's
	 * array) is adjusted given the values in the destination's
	 * previous array (essentially taking the numerical maximum of
	 * each).
	 */
	for (i = 0, j = 0 ; i < ndelegs && j < tcapdst->ndelegs ; i++) {
		/* fast-forward the destination's array */
		while (deleg_tmp[i].sched > tcapdst->delegations[j].sched) j++;
		if (deleg_tmp[i].sched != tcap_sched_info(tcapsrc)->sched && 
		    deleg_tmp[i].sched == tcapdst->delegations[j].sched) {
			/* due to tests in legal_transfer, we know the
			 * rhs is >= than the lhs */
			assert(deleg_tmp[i].prio <= tcapdst->delegations[j].prio);
			deleg_tmp[i].prio = tcapdst->delegations[j].prio;
		}
	}

	/* commit changes (if the transfer is successful) */
	if (__tcap_transfer(tcapdst, tcapsrc, cycles, tcap_sched_info(tcapdst)->prio, pooled)) return -1;
	//td->prio            = prio;
	memcpy(tcapdst->delegations, deleg_tmp, sizeof(struct tcap_sched_info) * ndelegs);
	tcapdst->ndelegs    = ndelegs;
	tcapdst->sched_info = td - deleg_tmp;

	return 0;
}

int
tcap_merge(struct tcap *dst, struct tcap *rm)
{
	struct tcap *td, *tr;

	tr = tcap_deref(&rm->budget);
	td = tcap_deref(&dst->budget);
	if (!tr || !td) return -1;

	if (tr != td) {
		if (tcap_transfer(dst, rm, tr->budget_local.cycles, 
				  tcap_sched_info(dst)->prio, 0)) return -1;
	}
	if (tcap_delete(tcap_sched_info(rm)->sched, rm)) return -1;

	return 0;
}

/* 
 * Is the newly activated thread of a higher priority than the current
 * thread?  Fast path called for each interrupt and asynchronous
 * thread invocation.
 */
int 
tcap_higher_prio(struct thread *activated, struct thread *curr)
{
	struct tcap *a, *c;
	int i, j;
	assert(activated && curr);

	a = tcap_deref(&activated->tcap_active);
	c = tcap_deref(&curr->tcap_active);
	/* invalid/inactive tcaps? */
	if (unlikely(!a)) return 0;
	if (unlikely(!c)) return 1;

	for (i = 0, j = 0 ; i < a->ndelegs && j < c->ndelegs ; ) {
		/* 
		 * These cases are for the case where the tcaps don't
		 * share a common scheduler (due to the partial order
		 * of schedulers), or different interrupt bind points.
		 */
		if (a->delegations[i].sched > c->delegations[j].sched) {
			j++;
		} else if (a->delegations[i].sched < c->delegations[j].sched) {
			i++; 
		} else { /* same shared scheduler! */
			if (a->delegations[i].prio > c->delegations[j].prio) return 0;
			i++; 
			j++;
		}
	}

	return 1;
}

void
tcap_elapsed(struct thread *t, unsigned int cycles)
{
	struct tcap *tc;

	tc = tcap_deref(&t->tcap_active);
	assert(tc);
	tcap_consume(tc, cycles);
}

/* TODO: percpu */
static struct spd *tcap_fountain = NULL;

int
tcap_fountain_create(struct spd *c)
{
	assert(c);
	tcap_fountain = c;
	return 0;
}

int 
tcap_tick_process(void)
{
	struct tcap *tc;
	struct spd  *c;
	extern u32_t cyc_per_tick;

	if (unlikely(!tcap_fountain)) return -1;
	c  = tcap_fountain;
	tc = &c->tcaps[0];
	tc->budget_local.cycles = cyc_per_tick;

	return 0;
}
