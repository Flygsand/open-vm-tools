/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/**
 * @file guestInfoPosix.c
 *
 * Contains POSIX-specific bits of GuestInfo collector library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef sun
# include <sys/systeminfo.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/sysctl.h>
#endif
#ifndef NO_DNET
# ifdef DNET_IS_DUMBNET
#  include <dumbnet.h>
# else
#  include <dnet.h>
# endif
#endif

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#ifdef __linux__
#   include <net/if.h>
#endif


/*
 * resolver(3) and IPv6:
 *
 * The ISC BIND resolver included various IPv6 implementations over time, but
 * unfortunately the ISC hadn't bumped __RES accordingly.  (__RES is -supposed-
 * to behave as a version datestamp for the resolver interface.)  Similarly
 * the GNU C Library forked resolv.h and made modifications of their own, also
 * without changing __RES.
 *
 * glibc 2.1.92 included a patch which provides IPv6 name server support by
 * embedding in6_addr pointers in _res._u._ext.  Since I only care about major
 * and minor numbers, though, I'm going to condition this impl. on glibc 2.2.
 *
 * ISC, OTOH, provided accessing IPv6 servers via a res_getservers API.
 * TTBOMK, this went public with BIND 8.3.0.  Unfortunately __RES wasn't
 * bumped for this release, so instead I'm going to assume that appearance with
 * that release of a new macro, RES_F_DNS0ERR, implies this API is available.
 * (For internal builds, we'll know instantly when a build breaks.  The down-
 * side is that this could cause some trouble for Open VM Tools users. ,_,)
 *
 * resolv.h version     IPv6 API        __RES
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * glibc 2.2+           _ext            19991006
 * BIND 8.3.0           getservers      19991006
 * BIND 8.3.4+          getservers      20030124(+?)
 *
 * To distinguish between the variants where __RES == 19991006, I'll
 * discriminate on the existence of new macros included with the appropriate
 * version.
 */

#if defined __GLIBC__
#   if __GLIBC_PREREQ(2,2)
#      define      RESOLVER_IPV6_EXT
#   endif // __GLIBC_PREREQ(2,2)
#elif (__RES > 19991006 || (__RES == 19991006 && defined RES_F_EDNS0ERR))
#   define      RESOLVER_IPV6_GETSERVERS
#endif // if defined __GLIBC__


#include "util.h"
#include "sys/utsname.h"
#include "sys/ioctl.h"
#include "vmware.h"
#include "hostinfo.h"
#include "getlibInt.h"
#include "debug.h"
#include "str.h"
#include "guest_os.h"
#include "guestApp.h"
#include "guestInfo.h"
#include "xdrutil.h"
#ifdef USE_SLASH_PROC
#   include "slashProc.h"
#endif
#include "netutil.h"
#include "file.h"


/*
 * Local functions
 */


#ifndef NO_DNET
static void RecordNetworkAddress(GuestNicV3 *nic, const struct addr *addr);
static int ReadInterfaceDetails(const struct intf_entry *entry, void *arg);
static Bool RecordResolverInfo(NicInfoV3 *nicInfo);
static void RecordResolverNS(DnsConfigInfo *dnsConfigInfo);
static Bool RecordRoutingInfo(NicInfoV3 *nicInfo);
#endif


/*
 ******************************************************************************
 * GuestInfoGetFqdn --                                                   */ /**
 *
 * @copydoc GuestInfo_GetFqdn
 *
 ******************************************************************************
 */

Bool
GuestInfoGetFqdn(int outBufLen,    // IN: length of output buffer
                 char fqdn[])      // OUT: fully qualified domain name
{
   ASSERT(fqdn);
   if (gethostname(fqdn, outBufLen) < 0) {
      g_debug("Error, gethostname failed\n");
      return FALSE;
   }

   return TRUE;
}


/*
 ******************************************************************************
 * GuestInfoGetNicInfo --                                                */ /**
 *
 * @copydoc GuestInfo_GetNicInfo
 *
 ******************************************************************************
 */

Bool
GuestInfoGetNicInfo(NicInfoV3 *nicInfo) // OUT
{
#ifndef NO_DNET
   intf_t *intf;

   /* Get a handle to read the network interface configuration details. */
   if ((intf = intf_open()) == NULL) {
      g_debug("Error, failed NULL result from intf_open()\n");
      return FALSE;
   }

   if (intf_loop(intf, ReadInterfaceDetails, nicInfo) < 0) {
      intf_close(intf);
      g_debug("Error, negative result from intf_loop\n");
      return FALSE;
   }

   intf_close(intf);

   if (!RecordResolverInfo(nicInfo)) {
      return FALSE;
   }

   if (!RecordRoutingInfo(nicInfo)) {
      return FALSE;
   }

   return TRUE;
#else
   return FALSE;
#endif
}


/*
 ******************************************************************************
 * GuestInfo_GetDiskInfo --                                              */ /**
 *
 * Uses wiper library to enumerate fixed volumes and lookup utilization data.
 *
 * @return Pointer to a GuestDiskInfo structure on success or NULL on failure.
 *         Caller should free returned pointer with GuestInfoFreeDiskInfo.
 *
 ******************************************************************************
 */

GuestDiskInfo *
GuestInfo_GetDiskInfo(void)
{
   return GuestInfoGetDiskInfoWiper();
}


/*
 * Local functions
 */


#ifndef NO_DNET
/*
 ******************************************************************************
 * RecordNetworkAddress --                                               */ /**
 *
 * @brief Massages a dnet(3)-style interface address (IPv4 or IPv6) and stores
 *        it as part of a GuestNicV3 structure.
 *
 * @param[in]  nic      Operand NIC.
 * @param[in]  addr     dnet(3) address.
 *
 ******************************************************************************
 */

static void
RecordNetworkAddress(GuestNicV3 *nic,           // IN: operand NIC
                     const struct addr *addr)   // IN: dnet(3) address to process
{
   struct sockaddr_storage ss;
   struct sockaddr *sa = (struct sockaddr *)&ss;

   memset(&ss, 0, sizeof ss);
   addr_ntos(addr, sa);
   GuestInfoAddIpAddress(nic, sa, addr->addr_bits, NULL, NULL);
}


/*
 ******************************************************************************
 * ReadInterfaceDetails --                                               */ /**
 *
 * @brief Callback function called by libdnet when iterating over all the NICs
 * on the host.
 *
 * @param[in]  entry    Current interface entry.
 * @param[in]  arg      Pointer to NicInfoV3 container.
 *
 * @note New GuestNicV3 structures are added to the NicInfoV3 structure.
 *
 * @retval 0    Success.
 * @retval -1   Failure.
 *
 ******************************************************************************
 */

static int
ReadInterfaceDetails(const struct intf_entry *entry,  // IN: current interface entry
                     void *arg)                       // IN: Pointer to the GuestNicList
{
   int i;
   NicInfoV3 *nicInfo = arg;

   ASSERT(entry);
   ASSERT(arg);

   if (entry->intf_type == INTF_TYPE_ETH &&
       entry->intf_link_addr.addr_type == ADDR_TYPE_ETH) {
      GuestNicV3 *nic = NULL;
      char macAddress[NICINFO_MAC_LEN];

      /*
       * There is a race where the guest info plugin might be iterating over the
       * interfaces while the OS is modifying them (i.e. by bringing them up
       * after a resume). If we see an ethernet interface with an invalid MAC,
       * then ignore it for now. Subsequent iterations of the gather loop will
       * pick up any changes.
       */
      if (entry->intf_link_addr.addr_type == ADDR_TYPE_ETH) {
         Str_Sprintf(macAddress, sizeof macAddress, "%s",
                     addr_ntoa(&entry->intf_link_addr));
         nic = GuestInfoAddNicEntry(nicInfo, macAddress, NULL, NULL);
         ASSERT_MEM_ALLOC(nic);

         /* Record the "primary" address. */
         if (entry->intf_addr.addr_type == ADDR_TYPE_IP ||
             entry->intf_addr.addr_type == ADDR_TYPE_IP6) {
            RecordNetworkAddress(nic, &entry->intf_addr);
         }

         /* Walk the list of alias's and add those that are IPV4 or IPV6 */
         for (i = 0; i < entry->intf_alias_num; i++) {
            const struct addr *alias = &entry->intf_alias_addrs[i];
            if (alias->addr_type == ADDR_TYPE_IP ||
                alias->addr_type == ADDR_TYPE_IP6) {
               RecordNetworkAddress(nic, alias);
            }
         }
      }
   }

   return 0;
}


/*
 ******************************************************************************
 * RecordResolverInfo --                                                 */ /**
 *
 * @brief Query resolver(3), mapping settings to DnsConfigInfo.
 *
 * @param[out] nicInfo  NicInfoV3 container.
 *
 * @retval TRUE         Values collected, attached to @a nicInfo.
 * @retval FALSE        Something went wrong.  @a nicInfo is unharmed.
 *
 ******************************************************************************
 */

static Bool
RecordResolverInfo(NicInfoV3 *nicInfo)  // OUT
{
   DnsConfigInfo *dnsConfigInfo = NULL;
   char namebuf[DNSINFO_MAX_ADDRLEN + 1];
   char **s;

   if (res_init() == -1) {
      return FALSE;
   }

   dnsConfigInfo = Util_SafeCalloc(1, sizeof *dnsConfigInfo);

   /*
    * Copy in the host name.
    */
   if (!GuestInfoGetFqdn(sizeof namebuf, namebuf)) {
      goto fail;
   }
   dnsConfigInfo->hostName =
      Util_SafeCalloc(1, sizeof *dnsConfigInfo->hostName);
   *dnsConfigInfo->hostName = Util_SafeStrdup(namebuf);

   /*
    * Repeat with the domain name.
    */
   dnsConfigInfo->domainName =
      Util_SafeCalloc(1, sizeof *dnsConfigInfo->domainName);
   *dnsConfigInfo->domainName = Util_SafeStrdup(_res.defdname);

   /*
    * Name servers.
    */
   RecordResolverNS(dnsConfigInfo);

   /*
    * Search suffixes.
    */
   for (s = _res.dnsrch; *s; s++) {
      DnsHostname *suffix;

      /* Check to see if we're going above our limit. See bug 605821. */
      if (dnsConfigInfo->searchSuffixes.searchSuffixes_len == DNSINFO_MAX_SUFFIXES) {
         g_message("%s: dns search suffix limit (%d) reached, skipping overflow.",
                   __FUNCTION__, DNSINFO_MAX_SUFFIXES);
         break;
      }

      suffix = XDRUTIL_ARRAYAPPEND(dnsConfigInfo, searchSuffixes, 1);
      ASSERT_MEM_ALLOC(suffix);
      *suffix = Util_SafeStrdup(*s);
   }

   /*
    * "Commit" dnsConfigInfo to nicInfo.
    */
   nicInfo->dnsConfigInfo = dnsConfigInfo;

   return TRUE;

fail:
   VMX_XDR_FREE(xdr_DnsConfigInfo, dnsConfigInfo);
   free(dnsConfigInfo);
   return FALSE;
}


/*
 ******************************************************************************
 * RecordResolverNS --                                                   */ /**
 *
 * @brief Copies name servers used by resolver(3) to @a dnsConfigInfo.
 *
 * @param[out] dnsConfigInfo    Destination DnsConfigInfo container.
 *
 ******************************************************************************
 */

static void
RecordResolverNS(DnsConfigInfo *dnsConfigInfo) // IN
{
   int i;

#if defined RESOLVER_IPV6_GETSERVERS
   {
      union res_sockaddr_union *ns;
      ns = Util_SafeCalloc(_res.nscount, sizeof *ns);
      if (res_getservers(&_res, ns, _res.nscount) != _res.nscount) {
         g_warning("%s: res_getservers failed.\n", __func__);
         return;
      }
      for (i = 0; i < _res.nscount; i++) {
         struct sockaddr *sa = (struct sockaddr *)&ns[i];
         if (sa->sa_family == AF_INET || sa->sa_family == AF_INET6) {
            TypedIpAddress *ip;

            /* Check to see if we're going above our limit. See bug 605821. */
            if (dnsConfigInfo->serverList.serverList_len == DNSINFO_MAX_SERVERS) {
               g_message("%s: dns server limit (%d) reached, skipping overflow.",
                         __FUNCTION__, DNSINFO_MAX_SERVERS);
               break;
            }

            ip = XDRUTIL_ARRAYAPPEND(dnsConfigInfo, serverList, 1);
            ASSERT_MEM_ALLOC(ip);
            GuestInfoSockaddrToTypedIpAddress(sa, ip);
         }
      }
   }
#else                                   // if defined RESOLVER_IPV6_GETSERVERS
   {
      /*
       * Name servers (IPv4).
       */
      for (i = 0; i < MAXNS; i++) {
         struct sockaddr_in *sin = &_res.nsaddr_list[i];
         if (sin->sin_family == AF_INET) {
            TypedIpAddress *ip;

            /* Check to see if we're going above our limit. See bug 605821. */
            if (dnsConfigInfo->serverList.serverList_len == DNSINFO_MAX_SERVERS) {
               g_message("%s: dns server limit (%d) reached, skipping overflow.",
                         __FUNCTION__, DNSINFO_MAX_SERVERS);
               break;
            }

            ip = XDRUTIL_ARRAYAPPEND(dnsConfigInfo, serverList, 1);
            ASSERT_MEM_ALLOC(ip);
            GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin, ip);
         }
      }
#   if defined RESOLVER_IPV6_EXT
      /*
       * Name servers (IPv6).
       */
      for (i = 0; i < MAXNS; i++) {
         struct sockaddr_in6 *sin6 = _res._u._ext.nsaddrs[i];
         if (sin6) {
            TypedIpAddress *ip;

            /* Check to see if we're going above our limit. See bug 605821. */
            if (dnsConfigInfo->serverList.serverList_len == DNSINFO_MAX_SERVERS) {
               g_message("%s: dns server limit (%d) reached, skipping overflow.",
                         __FUNCTION__, DNSINFO_MAX_SERVERS);
               break;
            }

            ip = XDRUTIL_ARRAYAPPEND(dnsConfigInfo, serverList, 1);
            ASSERT_MEM_ALLOC(ip);
            GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin6, ip);
         }
      }
#   endif                               //    if defined RESOLVER_IPV6_EXT
   }
#endif                                  // if !defined RESOLVER_IPV6_GETSERVERS
}


#ifdef USE_SLASH_PROC
/*
 ******************************************************************************
 * RecordRoutingInfoIPv4 --                                              */ /**
 *
 * @brief Query the IPv4 routing subsystem and pack up contents
 * (struct rtentry) into InetCidrRouteEntries.
 *
 * @param[out] nicInfo  NicInfoV3 container.
 *
 * @note Do not call this routine without first populating @a nicInfo 's NIC
 * list.
 *
 * @retval TRUE         Values collected, attached to @a nicInfo.
 * @retval FALSE        Something went wrong.  @a nicInfo is unharmed.
 *
 ******************************************************************************
 */

static Bool
RecordRoutingInfoIPv4(NicInfoV3 *nicInfo)
{
   GPtrArray *routes = NULL;
   guint i;
   Bool ret = FALSE;

   if ((routes = SlashProcNet_GetRoute()) == NULL) {
      return FALSE;
   }

   for (i = 0; i < routes->len; i++) {
      struct rtentry *rtentry;
      struct sockaddr_in *sin_dst;
      struct sockaddr_in *sin_gateway;
      struct sockaddr_in *sin_genmask;
      InetCidrRouteEntry *icre;
      uint32_t ifIndex;

      /* Check to see if we're going above our limit. See bug 605821. */
      if (nicInfo->routes.routes_len == NICINFO_MAX_ROUTES) {
         g_message("%s: route limit (%d) reached, skipping overflow.",
                   __FUNCTION__, NICINFO_MAX_ROUTES);
         break;
      }

      rtentry = g_ptr_array_index(routes, i);

      if ((rtentry->rt_flags & RTF_UP) == 0 ||
          !GuestInfoGetNicInfoIfIndex(nicInfo,
                                      if_nametoindex(rtentry->rt_dev),
                                      &ifIndex)) {
         continue;
      }

      icre = XDRUTIL_ARRAYAPPEND(nicInfo, routes, 1);
      ASSERT_MEM_ALLOC(icre);

      sin_dst = (struct sockaddr_in *)&rtentry->rt_dst;
      sin_gateway = (struct sockaddr_in *)&rtentry->rt_gateway;
      sin_genmask = (struct sockaddr_in *)&rtentry->rt_genmask;

      GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin_dst,
                                        &icre->inetCidrRouteDest);

      addr_stob((struct sockaddr *)sin_genmask,
                (uint16_t *)&icre->inetCidrRoutePfxLen);

      /*
       * Gateways are optional (ex: one can bind a route to an interface w/o
       * specifying a next hop address).
       */
      if (rtentry->rt_flags & RTF_GATEWAY) {
         TypedIpAddress *ip = Util_SafeCalloc(1, sizeof *ip);
         GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin_gateway, ip);
         icre->inetCidrRouteNextHop = ip;
      }

      /*
       * Interface, metric.
       */
      icre->inetCidrRouteIfIndex = ifIndex;
      icre->inetCidrRouteMetric = rtentry->rt_metric;
   }

   ret = TRUE;

   SlashProcNet_FreeRoute(routes);
   return ret;
}


/*
 ******************************************************************************
 * RecordRoutingInfoIPv6 --                                              */ /**
 *
 * @brief Query the IPv6 routing subsystem and pack up contents
 * (struct in6_rtmsg) into InetCidrRouteEntries.
 *
 * @param[out] nicInfo  NicInfoV3 container.
 *
 * @note Do not call this routine without first populating @a nicInfo 's NIC
 * list.
 *
 * @retval TRUE         Values collected, attached to @a nicInfo.
 * @retval FALSE        Something went wrong.  @a nicInfo is unharmed.
 *
 ******************************************************************************
 */

static Bool
RecordRoutingInfoIPv6(NicInfoV3 *nicInfo)
{
   GPtrArray *routes = NULL;
   guint i;
   Bool ret = FALSE;

   if ((routes = SlashProcNet_GetRoute6()) == NULL) {
      return FALSE;
   }

   for (i = 0; i < routes->len; i++) {
      struct sockaddr_storage ss;
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&ss;
      struct in6_rtmsg *in6_rtmsg;
      InetCidrRouteEntry *icre;
      uint32_t ifIndex = -1;

      /* Check to see if we're going above our limit. See bug 605821. */
      if (nicInfo->routes.routes_len == NICINFO_MAX_ROUTES) {
         g_message("%s: route limit (%d) reached, skipping overflow.",
                   __FUNCTION__, NICINFO_MAX_ROUTES);
         break;
      }

      in6_rtmsg = g_ptr_array_index(routes, i);

      if ((in6_rtmsg->rtmsg_flags & RTF_UP) == 0 ||
          !GuestInfoGetNicInfoIfIndex(nicInfo, in6_rtmsg->rtmsg_ifindex,
                                      &ifIndex)) {
         continue;
      }

      icre = XDRUTIL_ARRAYAPPEND(nicInfo, routes, 1);
      ASSERT_MEM_ALLOC(icre);

      /*
       * Destination.
       */
      sin6->sin6_family = AF_INET6;
      sin6->sin6_addr = in6_rtmsg->rtmsg_dst;
      GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin6,
                                        &icre->inetCidrRouteDest);

      icre->inetCidrRoutePfxLen = in6_rtmsg->rtmsg_dst_len;

      /*
       * Next hop.
       */
      if (in6_rtmsg->rtmsg_flags & RTF_GATEWAY) {
         TypedIpAddress *ip = Util_SafeCalloc(1, sizeof *ip);
         sin6->sin6_addr = in6_rtmsg->rtmsg_gateway;
         GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin6, ip);
         icre->inetCidrRouteNextHop = ip;
      }

      /*
       * Interface, metric.
       */
      icre->inetCidrRouteIfIndex = ifIndex;
      icre->inetCidrRouteMetric = in6_rtmsg->rtmsg_metric;
   }

   ret = TRUE;

   SlashProcNet_FreeRoute6(routes);
   return ret;
}


/*
 ******************************************************************************
 * RecordRoutingInfo --                                                  */ /**
 *
 * @brief Query the routing subsystem and pack up contents into
 * InetCidrRouteEntries.
 *
 * @param[out] nicInfo  NicInfoV3 container.
 *
 * @note Do not call this routine without first populating @a nicInfo 's NIC
 * list.
 *
 * @retval TRUE         Values collected, attached to @a nicInfo.
 * @retval FALSE        Something went wrong.
 *
 ******************************************************************************
 */

static Bool
RecordRoutingInfo(NicInfoV3 *nicInfo)
{
   Bool ret = TRUE;

   if (File_Exists("/proc/net/route") && !RecordRoutingInfoIPv4(nicInfo)) {
      g_warning("%s: Unable to collect IPv4 routing table.\n", __func__);
      ret = FALSE;
   }

   if (File_Exists("/proc/net/ipv6_route") && !RecordRoutingInfoIPv6(nicInfo)) {
      g_warning("%s: Unable to collect IPv6 routing table.\n", __func__);
      ret = FALSE;
   }

   return ret;
}

#else                                           // ifdef USE_SLASH_PROC
static Bool
RecordRoutingInfo(NicInfoV3 *nicInfo)
{
   return TRUE;
}
#endif                                          // else

#endif // ifndef NO_DNET
