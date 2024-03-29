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

/**
 * @file timeSync.c
 *
 * Plugin to handle time synchronization between the guest and host.
 *
 * There are two types of corrections this plugin makes: one time and periodic.
 *
 * Periodic time synchronization is done when tools.timeSync is enabled
 * (this corresponds with the Synchronize Host and Guest Time checkbox in
 * the toolbox).  When it is active time is corrected once per period
 * (typically every 60 seconds).
 *
 * One time corrections are done: at tools startup, resuming from suspend,
 * after disk shrink and other times when the guest has not been running
 * for a while.
 *
 * There are two basic methods for correcting the time: stepping and slewing.
 *
 * Stepping the time explictly sets the time in the guest to the time on
 * the host.  This a brute force approach that isn't very accurate.  Any
 * delay between deciding what to set the time to and actually setting the
 * time introduces error into the new time.  Additionally setting the time
 * backwards can confuse some applications.  During normal operation this
 * plugin only steps the time forward and only if the error is greater
 * than one second.
 *
 * Slewing time changes the rate of time advancement allowing errors to be
 * corrected smoothly (thus it is possible to correct time in the guest
 * being ahead of time on the host without time in the guest ever going
 * backwards).  An additional advantage is that only a relative change is
 * made, so delays in effecting a change don't introduce a large error
 * like they might with stepping the time.  One thing to note is that
 * windows has a notion of slewing being enabled/disabled independant of
 * whether the slew is set to nominal, so we track three states: disabled,
 * enabled-nominal, and enabled-active.
 *
 * Interacting with other time sync agents:
 *
 * When stepping it is relatively easy to co-exist with another time sync
 * agent.  We will only run into issues when we try to step the time at
 * exactly the same time as the other agent.  Since we are relatively
 * conservative about when to step, this is very unlikely.
 *
 * When slewing the time we will conflict much more directly with any
 * other time sync agent that is trying to slew the time since only one
 * slew rate can be active at any given time.  To play as nicely as
 * possible we only change the slew when necessary:
 *
 * 1. When starting the timesync loop reset the slew to nominal to clean
 *    up any odd state left behind a previous time sync agent.  For
 *    example vmware tools could have been running with a slew and then
 *    crashed.  Reseting to nominal gives us a reasonable starting point.
 *    An additional bonus is that on Windows turning on slewing (even when
 *    left at nominal) turns off windows' built in time synchronization
 *    according to MSDN.
 *
 * 2. When stopping the timesync loop disable slewing.  
 *
 * 3. When we stop slewing (either because we move to a host that doesn't
 *    support BDOOR_CMD_GETTIMEFULL_WITH_LAG or slew correction was
 *    disabled), reset the slew rate to nominal.
 *
 * 4. When stepping the time, reset slewing to nominal if it isn't
 *    already.
 *
 * 5. Avoid changing the slew in any other circumstance.  This allows a
 *    another agent to slew the time when we are not actively slewing.
 */

#include "timeSync.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "conf.h"
#include "msg.h"
#include "strutil.h"
#include "system.h"
#include "vmware/guestrpc/timesync.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


/* Sync the time once a minute. */
#define TIMESYNC_TIME 60
/* Correct PERCENT_CORRECTION percent of the error each period. */
#define TIMESYNC_PERCENT_CORRECTION 50

/* When measuring the difference between time on the host and time in the
 * guest we try up to TIMESYNC_MAX_SAMPLES times to read a sample
 * where the two host reads are within TIMESYNC_GOOD_SAMPLE_THRESHOLD
 * microseconds. */
#define TIMESYNC_MAX_SAMPLES 4
#define TIMESYNC_GOOD_SAMPLE_THRESHOLD 2000

/* Once the error drops below TIMESYNC_PLL_ACTIVATE, activate the PLL.
 * 500ppm error acumulated over a 60 second interval can produce 30ms of
 * error. */
#define TIMESYNC_PLL_ACTIVATE (30 * 1000) /* 30ms. */
/* If the error goes above TIMESYNC_PLL_UNSYNC, deactivate the PLL. */
#define TIMESYNC_PLL_UNSYNC (2 * TIMESYNC_PLL_ACTIVATE)
/* Period during which the frequency error of guest time is measured. */
#define TIMESYNC_CALIBRATION_DURATION (15 * 60 * US_PER_SEC) /* 15min. */

typedef enum TimeSyncState {
   TIMESYNC_INITIALIZING,
   TIMESYNC_STOPPED,
   TIMESYNC_RUNNING,
} TimeSyncState;

typedef enum TimeSyncSlewState {
   TimeSyncUncalibrated,
   TimeSyncCalibrating,
   TimeSyncPLL,
} TimeSyncSlewState;

typedef struct TimeSyncData {
   gboolean           slewActive;
   gboolean           slewCorrection;
   uint32             slewPercentCorrection;
   uint32             timeSyncPeriod;         /* In seconds. */
   TimeSyncState      state;
   TimeSyncSlewState  slewState;
   GSource           *timer;
} TimeSyncData;


static void TimeSyncSetSlewState(TimeSyncData *data, gboolean active);
static void TimeSyncResetSlew(TimeSyncData *data);

/**
 * Read the time reported by the Host OS.
 *
 * @param[out]  host                Time on the Host.
 * @param[out]  apparentError       Apparent time error = apparent - real.
 * @param[out]  apparentErrorValid  Did the platform inform us of apparentError.
 * @param[out]  maxTimeError        Maximum amount of error than can go.
 *                                  uncorrected.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncReadHost(int64 *host, int64 *apparentError, Bool *apparentErrorValid,
                 int64 *maxTimeError)
{
   Backdoor_proto bp;
   int64 maxTimeLag;
   int64 interruptLag;
   int64 hostSecs;
   int64 hostUsecs;
   Bool timeLagCall;

   /*
    * We need 3 things from the host, and there exist 3 different versions of
    * the calls (described further below):
    * 1) host time
    * 2) maximum time lag allowed (config option), which is a
    *    threshold that keeps the tools from being over eager about
    *    resetting the time when it is only a little bit off.
    * 3) interrupt lag (the amount that apparent time lags real time)
    *
    * First 2 versions of the call add interrupt lag to the maximum allowed
    * time lag, where as in the last call it is returned separately.
    *
    * Three versions of the call:
    *
    * - BDOOR_CMD_GETTIME: suffers from a 136-year overflow problem that
    *   cannot be corrected without breaking backwards compatibility with
    *   older Tools. So, we have the newer BDOOR_CMD_GETTIMEFULL, which is
    *   overflow safe.
    *
    * - BDOOR_CMD_GETTIMEFULL: overcomes the problem above.
    *
    * - BDOOR_CMD_GETTIMEFULL_WITH_LAG: Both BDOOR_CMD_GETTIMEFULL and
    *   BDOOR_CMD_GETTIME returns max lag limit as interrupt lag + the maximum
    *   allowed time lag. BDOOR_CMD_GETTIMEFULL_WITH_LAG separates these two
    *   values. This is helpful when synchronizing time backwards by slewing
    *   the clock.
    *
    * We use BDOOR_CMD_GETTIMEFULL_WITH_LAG first and fall back to
    * BDOOR_CMD_GETTIMEFULL or BDOOR_CMD_GETTIME.
    *
    * Note that BDOOR_CMD_GETTIMEFULL and BDOOR_CMD_GETTIMEFULL_WITH_LAG will
    * not touch EAX when it succeeds. So we check for errors by comparing EAX to
    * BDOOR_MAGIC, which was set by the call to Backdoor() prior to touching the
    * backdoor port.
    */
   bp.in.cx.halfs.low = BDOOR_CMD_GETTIMEFULL_WITH_LAG;
   Backdoor(&bp);
   if (bp.out.ax.word == BDOOR_MAGIC) {
      hostSecs = ((uint64)bp.out.si.word << 32) | bp.out.dx.word;
      interruptLag = bp.out.di.word;
      timeLagCall = TRUE;
      g_debug("Using BDOOR_CMD_GETTIMEFULL_WITH_LAG\n");
   } else {
      g_debug("BDOOR_CMD_GETTIMEFULL_WITH_LAG not supported by current host, "
              "attempting BDOOR_CMD_GETTIMEFULL\n");
      interruptLag = 0;
      timeLagCall = FALSE;
      bp.in.cx.halfs.low = BDOOR_CMD_GETTIMEFULL;
      Backdoor(&bp);
      if (bp.out.ax.word == BDOOR_MAGIC) {
         hostSecs = ((uint64)bp.out.si.word << 32) | bp.out.dx.word;
      } else {
         g_debug("BDOOR_CMD_GETTIMEFULL not supported by current host, "
                 "attempting BDOOR_CMD_GETTIME\n");
         bp.in.cx.halfs.low = BDOOR_CMD_GETTIME;
         Backdoor(&bp);
         hostSecs = bp.out.ax.word;
      }
   }
   hostUsecs = bp.out.bx.word;
   maxTimeLag = bp.out.cx.word;

   *host = hostSecs * US_PER_SEC + hostUsecs;
   *apparentError = -interruptLag;
   *apparentErrorValid = timeLagCall;
   *maxTimeError = maxTimeLag;

   if (hostSecs <= 0) {
      g_warning("Invalid host OS time: %"FMT64"d secs, %"FMT64"d usecs.\n\n",
                hostSecs, hostUsecs);
      return FALSE;
   }

   return TRUE;
}


/**
 * Read the Guest OS time and the Host OS time.
 *
 * There are three time domains that are revelant here:
 * 1. Guest time     - the time reported by the guest
 * 2. Apparent time  - the time reported by the virtualization layer
 * 3. Host time      - the time reported by the host operating system.
 *
 * This function reports the host time, the guest time and the difference
 * between apparent time and host time (apparentError).  The host and
 * guest time may be sampled multiple times to ensure an accurate reading.
 *
 * @param[out]  host                Time on the Host.
 * @param[out]  guest               Time in the Guest.
 * @param[out]  apparentError       Apparent time error = apparent - real.
 * @param[out]  apparentErrorValid  Did the platform inform us of apparentError.
 * @param[out]  maxTimeError        Maximum amount of error than can go.
 *                                  uncorrected.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncReadHostAndGuest(int64 *host, int64 *guest, 
                         int64 *apparentError, Bool *apparentErrorValid,
                         int64 *maxTimeError)
{
   int64 host1, host2, hostDiff;
   int64 tmpGuest, tmpApparentError, tmpMaxTimeError;
   Bool tmpApparentErrorValid;
   int64 bestHostDiff = MAX_INT64;
   int iter = 0;
   DEBUG_ONLY(static int64 lastHost = 0);

   *apparentErrorValid = FALSE;
   *host = *guest = *apparentError = *maxTimeError = 0;

   if (!TimeSyncReadHost(&host2, &tmpApparentError, 
                         &tmpApparentErrorValid, &tmpMaxTimeError)) {
      return FALSE;
   }

   do {
      iter++;
      host1 = host2;

      if (!TimeSync_GetCurrentTime(&tmpGuest)) {
         g_warning("Unable to retrieve the guest OS time: %s.\n\n", 
                   Msg_ErrString());
         return FALSE;
      }
      
      if (!TimeSyncReadHost(&host2, &tmpApparentError, 
                            &tmpApparentErrorValid, &tmpMaxTimeError)) {
         return FALSE;
      }
      
      if (host1 < host2) {
         hostDiff = host2 - host1;
      } else {
         hostDiff = 0;
      }

      if (hostDiff <= bestHostDiff) {
         bestHostDiff = hostDiff;
         *host = host1 + hostDiff / 2;
         *guest = tmpGuest;
         *apparentError = tmpApparentError;
         *apparentErrorValid = tmpApparentErrorValid;
         *maxTimeError = tmpMaxTimeError;
      }
   } while (iter < TIMESYNC_MAX_SAMPLES && 
            bestHostDiff > TIMESYNC_GOOD_SAMPLE_THRESHOLD);

   ASSERT(*host != 0 && *guest != 0);

#ifdef VMX86_DEBUG
   g_debug("Daemon: Guest vs host error %.6fs; guest vs apparent error %.6fs; "
           "limit=%.2fs; apparentError %.6fs; iter=%d error=%.6fs; "
           "%.6f secs since last update\n",
           (*guest - *host) / 1000000.0, 
           (*guest - *host - *apparentError) / 1000000.0, 
           *maxTimeError / 1000000.0, *apparentError / 1000000.0,
           iter, bestHostDiff / 1000000.0,
           (*host - lastHost) / 1000000.0);
   lastHost = *host;
#endif

   return TRUE;
}


/**
 * Set the guest OS time to the host OS time by stepping the time.
 *
 * @param[in]  data              Structure tracking time sync state.
 * @param[in]  adjustment        Amount to correct the guest time.
 */

gboolean
TimeSyncStepTime(TimeSyncData *data, int64 adjustment)
{
   Backdoor_proto bp;
   int64 before;
   int64 after;

   if (vmx86_debug) {
      TimeSync_GetCurrentTime(&before);
   }

   /* Stepping invalidates the current slew, reset to nominal. */
   TimeSyncSetSlewState(data, FALSE);

   if (!TimeSync_AddToCurrentTime(adjustment)) {
      return FALSE;
   }

   /* 
    * Tell timetracker to stop trying to catch up, since we have corrected
    * both the guest OS error and the apparent time error. 
    */
   bp.in.cx.halfs.low = BDOOR_CMD_STOPCATCHUP;
   Backdoor(&bp);

   if (vmx86_debug) {
      TimeSync_GetCurrentTime(&after);
      
      g_debug("Time changed by %"FMT64"dus from %"FMT64"d.%06"FMT64"d -> "
              "%"FMT64"d.%06"FMT64"d\n", adjustment,
              before / US_PER_SEC, before % US_PER_SEC, 
              after / US_PER_SEC, after % US_PER_SEC);
   }

   return TRUE;
}


/**
 * Slew the guest OS time advancement to correct the time.
 *
 * In addition to standard slewing (implemented via TimeSync_Slew), we
 * also support using an NTP style PLL to slew the time.  The PLL can take
 * a while to end up with an accurate measurement of the frequency error,
 * so before entering PLL mode we calibrate the frequency error over a
 * period of TIMESYNC_PLL_ACTIVATE seconds.  
 *
 * When using standard slewing, only correct slewPercentCorrection of the
 * error.  This is to avoid overcorrection when the error is mis-measured,
 * or overcorrection caused by the daemon waking up later than it is
 * supposed to leaving the slew in place for longer than anticpiated.
 *
 * @param[in]  data              Structure tracking time sync state.
 * @param[in]  adjustment        Amount to correct the guest time.
 */

static gboolean
TimeSyncSlewTime(TimeSyncData *data, int64 adjustment)
{
   static int64 calibrationStart;
   static int64 calibrationAdjustment;

   int64 now;
   int64 remaining = 0;
   int64 timeSyncPeriodUS = data->timeSyncPeriod * US_PER_SEC;
   int64 slewDiff = (adjustment * data->slewPercentCorrection) / 100;
   
   if (!TimeSync_GetCurrentTime(&now)) {
      return FALSE;
   }

   if (adjustment > TIMESYNC_PLL_UNSYNC && 
       data->slewState != TimeSyncUncalibrated) {
      g_debug("Adjustment too large (%"FMT64"d), resetting PLL state.\n", 
              adjustment);
      data->slewState = TimeSyncUncalibrated;
   }

   if (data->slewState == TimeSyncUncalibrated) {
      g_debug("Slewing time: adjustment %"FMT64"d\n", adjustment);
      if (!TimeSync_Slew(slewDiff, timeSyncPeriodUS, &remaining)) {
         data->slewState = TimeSyncUncalibrated;
         return FALSE;
      }
      if (adjustment < TIMESYNC_PLL_ACTIVATE && TimeSync_PLLSupported()) {
         g_debug("Starting PLL calibration.\n");
         calibrationStart = now;
         /* Starting out the calibration period we are adjustment behind,
          * but have already requested to correct slewDiff of that. */
         calibrationAdjustment = slewDiff - adjustment;
         data->slewState = TimeSyncCalibrating;
      }
   } else if (data->slewState == TimeSyncCalibrating) {
      if (now > calibrationStart + TIMESYNC_CALIBRATION_DURATION) {
         int64 ppmErr;
         /* Reset slewing to nominal and find out remaining slew. */
         TimeSync_Slew(0, timeSyncPeriodUS, &remaining);
         calibrationAdjustment += adjustment;
         calibrationAdjustment -= remaining;
         ppmErr = ((1000000 * calibrationAdjustment) << 16) / 
                   (now - calibrationStart);
         if (ppmErr >> 16 < 500 && ppmErr >> 16 > -500) {
            g_debug("Activating PLL ppmEst=%"FMT64"d (%"FMT64"d)\n", 
                    ppmErr >> 16, ppmErr);
            TimeSync_PLLUpdate(adjustment);
            TimeSync_PLLSetFrequency(ppmErr);
            data->slewState = TimeSyncPLL;
         } else {
            /* PPM error is too large to try the PLL. */
            g_debug("PPM error too large: %"FMT64"d (%"FMT64"d) "
                    "not activating PLL\n", ppmErr >> 16, ppmErr);
            data->slewState = TimeSyncUncalibrated;
         }
      } else {
         g_debug("Calibrating error: adjustment %"FMT64"d\n", adjustment);
         if (!TimeSync_Slew(slewDiff, timeSyncPeriodUS, &remaining)) {
            return FALSE;
         }
         calibrationAdjustment += slewDiff;
         calibrationAdjustment -= remaining;
      }
   } else {
      ASSERT(data->slewState == TimeSyncPLL);
      g_debug("Updating PLL: adjustment %"FMT64"d\n", adjustment);
      if (!TimeSync_PLLUpdate(adjustment)) {
         TimeSyncResetSlew(data);
      }
   }
   return TRUE;
}


/**
 * Reset the slew to nominal.
 *
 * @param[in]  data              Structure tracking time sync state.
 */

static void
TimeSyncResetSlew(TimeSyncData *data)
{
   int64 remaining;
   int64 timeSyncPeriodUS = data->timeSyncPeriod * US_PER_SEC;
   data->slewState = TimeSyncUncalibrated;
   TimeSync_Slew(0, timeSyncPeriodUS, &remaining);
   if (TimeSync_PLLSupported()) {
      TimeSync_PLLUpdate(0);
      TimeSync_PLLSetFrequency(0);
   }
}


/**
 * Update whether slewing is used for time correction.
 *
 * @param[in]  data              Structure tracking time sync state.
 * @param[in]  active            Is slewing active.
 */

static void
TimeSyncSetSlewState(TimeSyncData *data, gboolean active)
{
   if (active != data->slewActive) {
      g_debug(active ? "Starting slew.\n" : "Stopping slew.\n");
      if (!active) {
         TimeSyncResetSlew(data);
      }
      data->slewActive = active;
   }
}


/**
 * Set the guest OS time to the host OS time.
 *
 * @param[in]  slewCorrection    Is clock slewing enabled?
 * @param[in]  syncOnce          Is this function called in a loop?
 * @param[in]  allowBackwardSync Can we sync time backwards when doing syncOnce?
 * @param[in]  _data             Time sync data.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncDoSync(Bool slewCorrection,
               Bool syncOnce,
               Bool allowBackwardSync,
               void *_data)
{
   int64 guest, host;
   int64 gosError, apparentError, maxTimeError;
   Bool apparentErrorValid;
   TimeSyncData *data = _data;

   g_debug("Synchronizing time: "
           "syncOnce %d, slewCorrection %d, allowBackwardSync %d.\n",
           syncOnce, slewCorrection, allowBackwardSync);

   if (!TimeSyncReadHostAndGuest(&host, &guest, &apparentError, 
                                 &apparentErrorValid, &maxTimeError)) {
      return FALSE;
   }

   gosError = guest - host - apparentError;

   if (syncOnce) {

      /*
       * Non-loop behavior:
       *
       * Perform a step correction if:
       * 1) The guest OS error is behind by more than maxTimeError.
       * 2) The guest OS is ahead of the host OS.
       */

      if (gosError < -maxTimeError || 
          (gosError + apparentError > 0 && allowBackwardSync)) {
         g_debug("One time synchronization: stepping time.\n");
         if (!TimeSyncStepTime(data, -gosError + -apparentError)) {
            return FALSE;
         }
      } else {
         g_debug("One time synchronization: correction not needed.\n");
      }
   } else {

      /*
       * Loop behavior:
       *
       * If guest error is more than maxTimeError behind perform a step
       * correction.  Otherwise, if we can distinguish guest error from
       * apparent time error perform a slew correction .
       */

      TimeSyncSetSlewState(data, apparentErrorValid && slewCorrection);

      if (gosError < -maxTimeError) {
         g_debug("Periodic synchronization: stepping time.\n");
         if (!TimeSyncStepTime(data, -gosError + -apparentError)) {
            return FALSE;
         }
      } else if (slewCorrection && apparentErrorValid) {
         g_debug("Periodic synchronization: slewing time.\n");
         if (!TimeSyncSlewTime(data, -gosError)) {
            return FALSE;
         }
      }
   }

   return TRUE;
}


/**
 * Run the "time synchronization" loop.
 *
 * @param[in]  _data    Time sync data.
 *
 * @return always TRUE.
 */

static gboolean
ToolsDaemonTimeSyncLoop(gpointer _data)
{
   TimeSyncData *data = _data;

   ASSERT(data != NULL);

   if (!TimeSyncDoSync(data->slewCorrection, FALSE, FALSE, data)) {
      g_warning("Unable to synchronize time.\n");
   }

   return TRUE;
}


/**
 * Start the "time synchronization" loop.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  data     Time sync data.
 *
 * @return TRUE on success.
 */

static Bool
TimeSyncStartLoop(ToolsAppCtx *ctx,
                  TimeSyncData *data)
{
   ASSERT(data != NULL);
   ASSERT(data->state != TIMESYNC_RUNNING);
   ASSERT(data->timer == NULL);

   g_debug("Starting time sync loop.\n");

   /* 
    * Turn slew on and set it to nominal.  
    */
   TimeSyncResetSlew(data);

   g_debug("New sync period is %d sec.\n", data->timeSyncPeriod);

   if (!TimeSyncDoSync(data->slewCorrection, FALSE, FALSE, data)) {
      g_warning("Unable to synchronize time when starting time loop.\n");
   }

   data->timer = g_timeout_source_new(data->timeSyncPeriod * 1000);
   VMTOOLSAPP_ATTACH_SOURCE(ctx, data->timer, ToolsDaemonTimeSyncLoop, data, NULL);

   data->state = TIMESYNC_RUNNING;
   return TRUE;
}


/**
 * Stop the "time synchronization" loop.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  data     Time sync data.
 */

static void
TimeSyncStopLoop(ToolsAppCtx *ctx,
                 TimeSyncData *data)
{
   ASSERT(data != NULL);
   ASSERT(data->state == TIMESYNC_RUNNING);
   ASSERT(data->timer != NULL);

   g_debug("Stopping time sync loop.\n");

   TimeSyncSetSlewState(data, FALSE);
   TimeSync_DisableTimeSlew();

   g_source_destroy(data->timer);
   g_source_unref(data->timer);
   data->timer = NULL;

   data->state = TIMESYNC_STOPPED;
}


/**
 * Sync the guest's time with the host's.
 *
 * @param[in]  data     RPC request data.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncTcloHandler(RpcInData *data)
{
   Bool backwardSync = !strcmp(data->args, "1");
   TimeSyncData *syncData = data->clientData;

   if (!TimeSyncDoSync(syncData->slewCorrection, TRUE, backwardSync, syncData)) {
      return RPCIN_SETRETVALS(data, "Unable to sync time", FALSE);
   } else {
      return RPCIN_SETRETVALS(data, "", TRUE);
   }
}


/**
 * Parses boolean option string.
 *
 * @param[in]  string     Option string to be parsed.
 * @param[out] gboolean   Value of the option.
 *
 * @return TRUE on success.
 */

static gboolean
ParseBoolOption(const gchar *string,
                gboolean *value)
{
      if (strcmp(string, "1") == 0) {
         *value = TRUE;
      } else if (strcmp(string, "0") == 0) {
         *value = FALSE;
      } else {
         return FALSE;
      }
      return TRUE;
}

/**
 * Handles a "Set_Option" callback. Processes the time sync related options.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  option   Option being set.
 * @param[in]  value    Option value.
 * @param[in]  plugin   Plugin registration data.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncSetOption(gpointer src,
                  ToolsAppCtx *ctx,
                  const gchar *option,
                  const gchar *value,
                  ToolsPluginData *plugin)
{
   static gboolean syncBeforeLoop;
   TimeSyncData *data = plugin->_private;

   if (strcmp(option, TOOLSOPTION_SYNCTIME) == 0) {
      gboolean start;
      if (!ParseBoolOption(value, &start)) {
         return FALSE;
      }

      if (start && data->state != TIMESYNC_RUNNING) {
         /*
          * Try the one-shot time sync if time sync transitions from
          * 'off' to 'on' and TOOLSOPTION_SYNCTIME_ENABLE is turned on.
          * Note that during startup we receive TOOLSOPTION_SYNCTIME
          * before receiving TOOLSOPTION_SYNCTIME_ENABLE and so the
          * one-shot sync will not be done here. Nor should it because
          * the startup synchronization behavior is controlled by
          * TOOLSOPTION_SYNCTIME_STARTUP which is handled separately.
          */
         if (data->state == TIMESYNC_STOPPED && syncBeforeLoop) {
            TimeSyncDoSync(data->slewCorrection, TRUE, TRUE, data);
         }

         if (!TimeSyncStartLoop(ctx, data)) {
            g_warning("Unable to change time sync period.\n");
            return FALSE;
         }

      } else if (!start && data->state == TIMESYNC_RUNNING) {
         TimeSyncStopLoop(ctx, data);
      }

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_SLEWCORRECTION) == 0) {
      data->slewCorrection = strcmp(value, "0");
      g_debug("Daemon: Setting slewCorrection, %d.\n", data->slewCorrection);

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERCENTCORRECTION) == 0) {
      int32 percent;

      g_debug("Daemon: Setting slewPercentCorrection to %s.\n", value);
      if (!StrUtil_StrToInt(&percent, value)) {
         return FALSE;
      }
      if (percent <= 0 || percent > 100) {
         data->slewPercentCorrection = TIMESYNC_PERCENT_CORRECTION;
      } else {
         data->slewPercentCorrection = percent;
      }

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERIOD) == 0) {
      uint32 period;

      if (!StrUtil_StrToUint(&period, value)) {
         return FALSE;
      }

      if (period <= 0)
         period = TIMESYNC_TIME;

      /*
       * If the sync loop is running and the time sync period has changed,
       * restart the loop with the new period value. If the sync loop is
       * not running, just remember the new sync period value.
       */
      if (period != data->timeSyncPeriod) {
         data->timeSyncPeriod = period;

         if (data->state == TIMESYNC_RUNNING) {
            TimeSyncStopLoop(ctx, data);
            if (!TimeSyncStartLoop(ctx, data)) {
               g_warning("Unable to change time sync period.\n");
               return FALSE;
            }
         }
      }

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_STARTUP) == 0) {
      static gboolean doneAlready = FALSE;
      gboolean doSync;

      if (!ParseBoolOption(value, &doSync)) {
         return FALSE;
      }

      if (doSync && !doneAlready &&
          !TimeSyncDoSync(data->slewCorrection, TRUE, TRUE, data)) {
         g_warning("Unable to sync time during startup.\n");
         return FALSE;
      }

      doneAlready = TRUE;

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_ENABLE) == 0) {
      if (!ParseBoolOption(value, &syncBeforeLoop)) {
         return FALSE;
      }

   } else {
      return FALSE;
   }

   return TRUE;
}


/**
 * Handles a shutdown callback; cleans up internal plugin state.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  plugin   Plugin registration data.
 */

static void
TimeSyncShutdown(gpointer src,
                 ToolsAppCtx *ctx,
                 ToolsPluginData *plugin)
{
   TimeSyncData *data = plugin->_private;

   if (data->state == TIMESYNC_RUNNING) {
      TimeSyncStopLoop(ctx, data);
   }

   g_free(data);
}


/**
 * Plugin entry point. Initializes internal state and returns the registration
 * data.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "timeSync",
      NULL,
      NULL
   };

   TimeSyncData *data = g_malloc(sizeof (TimeSyncData));
   RpcChannelCallback rpcs[] = {
      { TIMESYNC_SYNCHRONIZE, TimeSyncTcloHandler, data, NULL, NULL, 0 }
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_SET_OPTION, TimeSyncSetOption, &regData },
      { TOOLS_CORE_SIG_SHUTDOWN, TimeSyncShutdown, &regData }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   data->slewActive = FALSE;
   data->slewCorrection = FALSE;
   data->slewPercentCorrection = TIMESYNC_PERCENT_CORRECTION;
   data->state = TIMESYNC_INITIALIZING;
   data->slewState = TimeSyncUncalibrated;
   data->timeSyncPeriod = TIMESYNC_TIME;
   data->timer = NULL;

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   regData._private = data;

   return &regData;
}

