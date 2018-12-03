
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
#include "gpio_tests.h"

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"

// Termination state
extern sig_atomic_t terminationRequired;

// Determine how many GPIO pairs we have to test
static const int numGPIOPairs = sizeof(gpioPairs) / (sizeof(GPIO_Id) * 2);

static const int numGPIOTestLevels = sizeof(gpioTestLevels) / sizeof(GPIO_Value_Type);

static int gpioOutputFd = -1;
static int gpioInputFd = -1;

bool GPIOTestPassed(void) {

	// Sleep so we can see the testing LED color (white)
	sleep(1);

	bool allTestsPassed = true;
	bool testsPassed = true;

	if (numGPIOPairs == 0) {
		return allTestsPassed;
	}

	// Iterate over the GPIO array and for each pair set one as input and the other as output
	for (int i = 0; i < numGPIOPairs; i++) {
		testsPassed = test_GPIO_Pairs(gpioPairs[i].gpioX, gpioPairs[i].gpioY);
		if (!testsPassed) {
			allTestsPassed = false;
		}
		// Swap the GPIO pairs to test the opposite direction
		testsPassed = test_GPIO_Pairs(gpioPairs[i].gpioY, gpioPairs[i].gpioX);
		if (!testsPassed) {
			allTestsPassed = false;
		}
	}

	return allTestsPassed;
}

bool test_GPIO_Pairs(GPIO_Id outputGPIO, GPIO_Id inputGPIO) {

	bool testsPassed = true;

	// Define a variable to use when we read the state of the input GPIO
	static GPIO_Value_Type newGPIOState;

#ifdef  SHOW_DEBUG
	Log_Debug("TEST INFO: Testing GPIO_%d --> GPIO_%d\n", outputGPIO, inputGPIO);
#endif

	// Open outputGPIO for output
	gpioOutputFd = GPIO_OpenAsOutput(outputGPIO, GPIO_OutputMode_PushPull, GPIO_Value_High);
	if (gpioOutputFd < 0) {
		Log_Debug("ERROR: Could not open GPIO_%d: %s (%d).\n", outputGPIO, strerror(errno), errno);
		terminationRequired = true;
		return false;
	}

	// Open inputGPIO for input
	gpioInputFd = GPIO_OpenAsInput(inputGPIO);
	if (gpioInputFd < 0) {
		Log_Debug("ERROR: Could not open GPIO_%d: %s (%d).\n", inputGPIO, strerror(errno), errno);
		terminationRequired = true;
		return false;
	}

	// Cycle through all the differnt GPIO levels we want to test
	for (int y = 0; y < numGPIOTestLevels; y++) {

		int result = GPIO_SetValue(gpioOutputFd, gpioTestLevels[y]);
		if (result != 0) {
			Log_Debug("ERROR: Could not set GPIO_%d output value %d: %s (%d).\n", outputGPIO, gpioTestLevels[y], strerror(errno), errno);
			terminationRequired = true;
			return false;
		}

		// read inputGPIO and validate correct level
		if (GPIO_GetValue(gpioInputFd, &newGPIOState) != -1) {
			if (newGPIOState != gpioTestLevels[y]) {
				testsPassed = false;
				Log_Debug("TEST FAILURE: Validation Failed!  Read %d from GPIO_%d, expected %d\n", newGPIOState, inputGPIO, gpioTestLevels[y]);
			}
		}
		else {
			Log_Debug("TEST FAILURE: Could not read GPIO state for GPIO_%d\n", inputGPIO);
			terminationRequired = true;
			return false;
		}
	}

	// Close the file descriptors and set them to an invalid value -1
	CloseFdAndPrintError(gpioOutputFd, "Output GPIO");
	gpioOutputFd = -1;
	CloseFdAndPrintError(gpioInputFd, "Input GPIO");
	gpioInputFd = -1;
	return testsPassed;
}
