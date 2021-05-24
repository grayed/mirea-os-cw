/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Theo de Raadt
 * Copyright (c) 2017 YOUR NAME <email@example.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <sys/types.h>
#include <sys/timeout.h>
#include <sys/task.h>

#ifndef PGEN_TASKNAME
#define PGEN_TASKNAME "pgen"
#endif

void	cwnetattach(int);
int	cwnetioctl(struct ifnet *, u_long, caddr_t);
void	cwnetqstart(struct ifqueue *);
int	cwnet_clone_create(struct if_clone *, int);
int	cwnet_clone_destroy(struct ifnet *);
int	cwnet_media_change(struct ifnet *);
void	cwnet_media_status(struct ifnet *, struct ifmediareq *);


/*
 * Структура описывающая состояние генератора
 * 
 */

struct cwnet_softc {
	struct arpcom		sc_ac;
	struct ifmedia		sc_media;
	struct task		sc_t;
	struct timeout		sc_to;
	//struct taskq		sc_tq;
};

/*################################################################
 *
 *  Герератор пакетов
 *
 *################################################################*/


static void pgen_start(struct cwnet_softc *sc);
static void pgen_gen_packs(void *arg);
static void pgen_gen_pack(void *arg);
static void pgen_set(struct cwnet_softc	*sc);
static void pgen_destroy(struct cwnet_softc *sc);

static char sample[] = { 
		//destination MAC, to be overriden
		0xFF,	0xFF,	0xFF,	0xFF,	0xFF, 	0xFF,
		//source MAC
		0xFC,	0xFA,	0xFB,	0xFF,	0xFF, 	0xFF,
		//length 
		0x00,	0x34,   
		//source IPv4
		0x7F, 	0x00, 	0x00, 	0x01,  
		//destination IPv4
		0x7F, 	0x00, 	0x00, 	0x01, 
		//zeros|protocol|udp length
		0x00, 	0x11, 	0x00, 	0x28, 
		// source port |destination port
		0x00,	0x01,	0x00,	0x02,
		//length	|   checksum
		0x00,	0x20,   0x00,	0x00,
		//data
		0x00,	0x01,	0x02,	0x03, 	
		0x04,	0x05,	0x06,	0x07,
		0x08,	0x09,	0x0A,	0x0B,
		0x0C,	0x1D,	0x1E,	0x1F,
		0x10,	0x11,	0x12, 	0x13,
		0x14,	0x15,	0x16,	0x17,	
		0x18,	0x19,	0x1A,	0x1B,
		0x1C,	0x1D,	0x1E,	0x1F,
		//checksum
		0x00,	0x00,	0x00,	0x00
};

/*
 * функция запускающая генератор пакетов
 */
static void
pgen_start(struct cwnet_softc *sc)
{
	timeout_add_sec(&sc->sc_to, 1);
}

/*
 * функция старутющая генерацию пакетов в отдельном потоке ядра
 */
static void
pgen_gen_packs(void *arg)
{
	struct cwnet_softc *sc = arg;

	task_add(systq, &sc->sc_t);
}

/*
 * функция генерирующая один пакет
 */
static void
pgen_gen_pack(void *arg)
{
	struct cwnet_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf *m;
	struct ether_header *eh;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();

	// printf("gen\n");

	m = MCLGETI(NULL, M_WAITOK, ifp, sizeof(sample));
	m->m_len = m->m_pkthdr.len = sizeof(sample);
	eh = mtod(m, struct ether_header *);
	memcpy(eh, sample, sizeof(sample));
	memcpy(eh->ether_dhost, LLADDR(ifp->if_sadl), 6);

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);
	pgen_start(sc);
}

/*
 *	инициализирует все поля генератора
 */
static void
pgen_set(struct cwnet_softc *sc)
{
 	task_set(&sc->sc_t, pgen_gen_pack, sc);
	timeout_set(&sc->sc_to, pgen_gen_packs, sc);
}

/*
 * освобождение ресерусов
 */
static void
pgen_destroy(struct cwnet_softc	*sc)
{
	task_del(systq, &sc->sc_t);
	timeout_del(&sc->sc_to);
}

/*################################################################################################
 *
 *
 *
 *#################################################################################################*/



struct if_clone	cwnet_cloner =
    IF_CLONE_INITIALIZER("cwnet", cwnet_clone_create, cwnet_clone_destroy);


int
cwnet_media_change(struct ifnet *ifp)
{
	return (0);
}

void
cwnet_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID | IFM_ACTIVE;
}

void
cwnetattach(int ncwnet)
{
	if_clone_attach(&cwnet_cloner);
}

int
cwnet_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet		*ifp;
	struct cwnet_softc	*sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = &sc->sc_ac.ac_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "cwnet%d", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_MPSAFE;
	ether_fakeaddr(ifp);

	ifp->if_softc = sc;
	ifp->if_ioctl = cwnetioctl;
	ifp->if_qstart = cwnetqstart;

	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	ifp->if_capabilities = IFCAP_VLAN_MTU;

	ifmedia_init(&sc->sc_media, 0, cwnet_media_change,
	    cwnet_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	pgen_set(sc);
	if_attach(ifp);
	ether_ifattach(ifp);
	return (0);
}

int
cwnet_clone_destroy(struct ifnet *ifp)
{
	struct cwnet_softc	*sc = ifp->if_softc;

	pgen_destroy(sc);
	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);
	free(sc, M_DEVBUF, sizeof(*sc));
	return (0);
}

/*
 * The bridge has magically already done all the work for us,
 * and we only need to discard the packets.
 */
void
cwnetqstart(struct ifqueue *ifq)
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	struct cwnet_softc	*sc;

	ifp = ifq->ifq_if;
	sc = ifp->if_softc;
	pgen_start(sc);
	while ((m = ifq_dequeue(ifq)) != NULL) {
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif /* NBPFILTER > 0 */

		m_freem(m);
	}
}

int
cwnetioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct cwnet_softc	*sc = (struct cwnet_softc *)ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	int			 error = 0, link_state;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			ifp->if_flags |= IFF_RUNNING;
			link_state = LINK_STATE_UP;
		} else {
			ifp->if_flags &= ~IFF_RUNNING;
			link_state = LINK_STATE_DOWN;
		}
		if (ifp->if_link_state != link_state) {
			ifp->if_link_state = link_state;
			if_link_state_change(ifp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}
	return (error);
}
