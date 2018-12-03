
#include <applibs/gpio.h>
#include <applibs/log.h>

#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "platform.h"
#include "led_tests.h"

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"


// Termination state
extern sig_atomic_t terminationRequired;

// Calculate how many LEDs we will control
int static numLedGPIOs = sizeof(gpioTestList) / sizeof(GPIO_Id);

int static fdList[sizeof(gpioTestList) / sizeof(GPIO_Id)];

void cleanupLedFdList(void) {

	for (int i = 0; i < numLedGPIOs; i++) {

		if (fdList[i] != -1) {

			// Turn off all LEDs
			int result = GPIO_SetValue(fdList[i], GPIO_Value_High);

			if (result != 0) {
				Log_Debug("TEST FAILURE: Could not set GPIO_%d output value %d: %s (%d).\n", gpioTestList[i], GPIO_Value_High, strerror(errno), errno);
				terminationRequired = true;
			}

			
			// Close the file descriptors and set them to an invalid value -1
			CloseFdAndPrintError(fdList[i], "LED GPIO");
			fdList[i] = -1;
		}
	}
}

bool populateLedFdList(void) {

	bool returnValue = true;

	// If we don't have any LEDs for the LED test, then just return success!
	if (numLedGPIOs == 0) {
		return true;
	}

	// For each GPIO in the list, create a file descriptor and then turn it on/off 

	for (int i = 0; i < numLedGPIOs; i++) {

		fdList[i] = GPIO_OpenAsOutput(gpioTestList[i], GPIO_OutputMode_PushPull, GPIO_Value_High);
		if (fdList[i] < 0) {
			Log_Debug("TEST FAILURE: Could not open GPIO_%d: %s (%d).\n", gpioTestList[i], strerror(errno), errno);
			returnValue = false;
			break;
		}
	}

	return returnValue;
}

void ledTestChangeLeds(GPIO_Value newState) {

	// Setup ts to ~0.4 seconds
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = LED_DELAY_NS;

	// For each GPIO in the list turn it on/off 

	for (int i = 0; i < numLedGPIOs; i++) {

		int result = GPIO_SetValue(fdList[i], newState);
		// pause to turn the LEDs on/off in a sequence
		nanosleep(&ts, NULL);
		result = GPIO_SetValue(fdList[i], GPIO_Value_High);

		if (result != 0) {
			Log_Debug("TEST FAILURE: Could not set GPIO_%d output value %d: %s (%d).\n", gpioTestList[i], newState, strerror(errno), errno);
			terminationRequired = true;
		}
	}
}