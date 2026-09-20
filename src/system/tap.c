/*
	SlimeVR Code is placed under the MIT license
	Copyright (c) 2025 SlimeVR Contributors

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "globals.h"
#include "system.h"
#include "status.h"
#include "tap.h"

LOG_MODULE_REGISTER(tap, LOG_LEVEL_INF);

#if CONFIG_USE_IMU_TAP

// Taps are counted like button presses, the action is dispatched after a
// 400ms idle window through the same sys_reset_mode used by reset counting:
// 1 tap = none, 2 taps = calibration, 3 taps = pairing reset, 4+ taps = DFU
K_SEM_DEFINE(tap_sem, 0, 1);
static int tap_presses = 0;
static int64_t last_tap = 0;

void sys_tap_event(int presses)
{
	tap_presses += presses;
	last_tap = k_uptime_get();
	k_sem_give(&tap_sem);
}

static void tap_thread(void)
{
	while (1)
	{
		k_sem_take(&tap_sem, K_FOREVER);
		set_status(SYS_STATUS_BUTTON_PRESSED, true); // treat taps like button presses, delays sleep transitions
		while (k_uptime_get() - last_tap <= 400)
			k_msleep(20);
		int presses = tap_presses;
		tap_presses = 0;
		set_status(SYS_STATUS_BUTTON_PRESSED, false);
		LOG_INF("Tap was performed %d times", presses);
		sys_reset_mode(presses - 1);
	}
}

K_THREAD_DEFINE(tap_thread_id, 512, tap_thread, NULL, NULL, NULL, TAP_THREAD_PRIORITY, 0, 0);

#else

void sys_tap_event(int presses) {}

#endif
