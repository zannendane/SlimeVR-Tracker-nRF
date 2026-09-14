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
#include <math.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "system/status.h"
#include "system/tap.h"

#include "tap_detect.h"

LOG_MODULE_REGISTER(tap_detect, LOG_LEVEL_INF);

#if CONFIG_USE_IMU_TAP

// A tap is a short dynamic acceleration spike on top of the gravity baseline.
// The baseline tracks the (slowly changing) magnitude at rest, so the detector
// works at any ODR and in any sensor power mode
static float baseline = 1.0f;
static float last_gyro_mag = 0.0f;
static bool in_shock = false;
static float shock_duration = 0.0f; // seconds, counted in samples (FIFO is processed in bursts)
static int64_t refractory_until = 0;

void tap_detect_reset(void)
{
	baseline = 1.0f;
	last_gyro_mag = 0.0f;
	in_shock = false;
	shock_duration = 0.0f;
	// ignore input while the IMU output and the baseline settle after (re)init
	refractory_until = k_uptime_get() + 250;
}

void tap_detect_gyro(const float g[3])
{
	last_gyro_mag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
}

void tap_detect_process(const float a[3], float dt)
{
	if (get_status(SYS_STATUS_CALIBRATION_RUNNING))
		return; // taps are not user input while calibrating

	float mag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
	// slow baseline, ~0.5s time constant
	float alpha = dt / (0.5f + dt);
	baseline += alpha * (mag - baseline);
	float dev = fabsf(mag - baseline);
	float threshold = CONFIG_IMU_TAP_THRESHOLD / 1000.0f;
	int64_t now = k_uptime_get();

	if (!in_shock)
	{
		if (now < refractory_until || last_gyro_mag > CONFIG_IMU_TAP_GYRO_GATE)
			return; // ignore input shortly after a tap and while rotating
		if (dev > threshold)
		{
			in_shock = true;
			shock_duration = dt;
		}
		return;
	}

	shock_duration += dt;
	if (dev < threshold / 2.0f)
	{
		in_shock = false;
		if (shock_duration < 0.25f) // longer disturbances are handling, not taps
		{
			refractory_until = now + 100;
			LOG_INF("Tap detected");
			sys_tap_event(1);
		}
	}
	else if (shock_duration > 0.5f)
	{
		in_shock = false; // stuck above threshold, abandon
	}
}

#else

void tap_detect_reset(void) {}
void tap_detect_gyro(const float g[3]) {}
void tap_detect_process(const float a[3], float dt) {}

#endif
