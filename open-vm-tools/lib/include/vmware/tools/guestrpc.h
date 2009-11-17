/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef _VMWARE_TOOLS_GUESTRPC_H_
#define _VMWARE_TOOLS_GUESTRPC_H_

/**
 * @file guestrpc.h
 *
 *    Defines the interface between applications and the underlying GuestRPC
 *    channel. The goal is to have an abstraction so applications can run over
 *    the backdoor, VMCI sockets or TCP/IP sockets by just picking up the
 *    desired channel at runtime, without the need to modify the code.
 *
 *    For this reason, the behavior of all channels is modeled after the RpcIn
 *    channel currently used in Tools, so the socket-based channels won't
 *    provide much better functionality than what the backdoor provides (aside
 *    from being interrupt-based rather than poll-based).
 *
 * @addtogroup vmtools_guestrpc
 * @{
 */

#include <glib.h>
#include "vmware/tools/utils.h"

G_BEGIN_DECLS

/** Alias for RpcChannel_SetRetVals. */
#define RPCIN_SETRETVALS RpcChannel_SetRetVals

struct RpcChannel;

/** Data structure passed to RPC callbacks. */
typedef struct RpcInData {
   /** RPC name. */
   const char *name;
   /**
    * RPC arguments. Either the raw argument data, or de-serialized XDR data
    * in case @a xdrIn was provided in the registration data.
    */
   const char *args;
   /** Size of raw argument data, in bytes. */
   size_t argsSize;
   /**
    * Data to be returned to the caller, or pointer to XDR structure if
    * @a xdrOut was provided in the registration data.
    */
   char *result;
   /** Length in bytes of raw data being returned (ignored for XDR structures). */
   size_t resultLen;
   /**
    * Whether the RPC library should free the contents of the @a result
    * field (using vm_free()).
    */
   gboolean freeResult;
   /** Application context. */
   void *appCtx;
   /** Client data specified in the registration data. */
   void *clientData;
} RpcInData;


/**
 * Type for RpcIn callbacks. The callback function is responsible for
 * allocating memory for the result string.
 */
typedef gboolean (*RpcIn_Callback)(RpcInData *data);


/** Defines the registration data for a GuestRPC application. */
typedef struct RpcChannelCallback {
   /** String identifying the RPC message. */
   const char       *name;
   /** Function to call when data arrives. */
   RpcIn_Callback    callback;
   /** Data to provide to callback function. */
   gpointer          clientData;
   /** If not NULL, the input data will be deserialized using this function. */
   gpointer          xdrIn;
   /**
    * If not NULL, the output data will be serialized using this function. The
    * output data should be stored in the @a result field of the RpcInData
    * structure, and should have been allocated with glib's @a g_malloc() if
    * @a freeResult is TRUE.
    */
   gpointer          xdrOut;
   /**
    * If xdrIn is not NULL, this should be the amount of memory to allocate
    * for deserializing the input data.
    */
   size_t            xdrInSize;
} RpcChannelCallback;


typedef gboolean (*RpcChannelStartFn)(struct RpcChannel *);
typedef void (*RpcChannelStopFn)(struct RpcChannel *);
typedef void (*RpcChannelShutdownFn)(struct RpcChannel *);
typedef gboolean (*RpcChannelSendFn)(struct RpcChannel *,
                                     char *data,
                                     size_t dataLen,
                                     char **result,
                                     size_t *resultLen);


/**
 * Signature for the callback function called after a channel reset.
 *
 * @param[in]  chan     The RPC channel.
 * @param[in]  success  Whether reset was successful.
 * @param[in]  data     Client data.
 */
typedef void (*RpcChannelResetCb)(struct RpcChannel *chan,
                                  gboolean success,
                                  gpointer data);


/** Defines the interface between the application and the RPC channel. */
typedef struct RpcChannel {
   RpcChannelStartFn       start;
   RpcChannelStopFn        stop;
   RpcChannelSendFn        send;
   /* Private section: don't use the fields below directly. */
   RpcChannelShutdownFn    shutdown;
   gchar                  *appName;
   GHashTable             *rpcs;
   GMainContext           *mainCtx;
   GSource                *resetCheck;
   gpointer                appCtx;
   RpcChannelCallback      resetReg;
   RpcChannelResetCb       resetCb;
   gpointer                resetData;
   gboolean                rpcError;
   guint                   rpcErrorCount;
   gpointer                _private;
} RpcChannel;


/**
 * Wrapper for the start function of an RPC channel struct.
 *
 * @param[in]  chan        The RPC channel instance.
 *
 * @return TRUE on success.
 */

G_INLINE_FUNC gboolean
RpcChannel_Start(RpcChannel *chan)
{
   g_return_val_if_fail(chan != NULL, FALSE);
   g_return_val_if_fail(chan->start != NULL, FALSE);

   return chan->start(chan);
}


/**
 * Wrapper for the stop function of an RPC channel struct.
 *
 * @param[in]  chan        The RPC channel instance.
 */

G_INLINE_FUNC void
RpcChannel_Stop(RpcChannel *chan)
{
   g_return_if_fail(chan != NULL);
   g_return_if_fail(chan->stop != NULL);

   chan->stop(chan);
}


/**
 * Wrapper for the send function of an RPC channel struct.
 *
 * @param[in]  chan        The RPC channel instance.
 * @param[in]  data        Data to send.
 * @param[in]  dataLen     Number of bytes to send.
 * @param[out] result      Response from other side.
 * @param[out] resultLen   Number of bytes in response.
 *
 * @return The status from the remote end (TRUE if call was successful).
 */

G_INLINE_FUNC gboolean
RpcChannel_Send(RpcChannel *chan,
                char *data,
                size_t dataLen,
                char **result,
                size_t *resultLen)
{
   g_return_val_if_fail(chan != NULL, FALSE);
   g_return_val_if_fail(chan->send != NULL, FALSE);

   return chan->send(chan, data, dataLen, result, resultLen);
}

gboolean
RpcChannel_BuildXdrCommand(const char *cmd,
                           void *xdrProc,
                           void *xdrData,
                           char **result,
                           size_t *resultLen);

gboolean
RpcChannel_Destroy(RpcChannel *chan);

gboolean
RpcChannel_Dispatch(RpcInData *data);

void
RpcChannel_Setup(RpcChannel *chan,
                 const gchar *appName,
                 GMainContext *mainCtx,
                 gpointer appCtx,
                 RpcChannelResetCb resetCb,
                 gpointer resetData);

void
RpcChannel_RegisterCallback(RpcChannel *chan,
                            RpcChannelCallback *rpc);

gboolean
RpcChannel_SetRetVals(RpcInData *data,
                      char *result,
                      gboolean retVal);

void
RpcChannel_UnregisterCallback(RpcChannel *chan,
                              RpcChannelCallback *rpc);


RpcChannel *
RpcChannel_NewBackdoorChannel(GMainContext *mainCtx);

G_END_DECLS

/** @} */

#endif
