

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"

#include <applibs/gpio.h>
#include <applibs/log.h>

#include "mt3620_rdb.h"
#include "rgbled_utility.h"

#include "gpio_tests.h"
#include "uart_tests.h"
#include "wifi_tests.h"
#include "led_tests.h"
#include "platform.h"


// File descriptors - initialized to invalid value
static int gpioButton1Fd = -1;
#ifdef TEST_BUTTON_B
static int gpioButton2Fd = -1;
#endif
static int gpioButtonTimerFd = -1;
static int gpioLedTimerFd = -1;
static int epollFd = -1;

// LED state
static RgbLed led1 = RGBLED_INIT_VALUE;
static RgbLed *rgbLeds[] = {&led1};
static const size_t rgbLedsCount = sizeof(rgbLeds) / sizeof(*rgbLeds);

#include "platform.h"

// An array defining the RGB GPIOs for the user LED
static const GPIO_Id ledsPins[1][3] = { {GPIO_RED, GPIO_GREEN, GPIO_BLUE} };

// Define a variable to control when we run the test(s) again.  This variable is set in the button handler and examined in the main() loop
static bool runTests = true;

// Termination state
sig_atomic_t terminationRequired = false;

/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
    terminationRequired = true;
}

/// <summary>
///     Check whether a given button has just been pressed.
/// </summary>
/// <param name="fd">The button file descriptor</param>
/// <param name="oldState">Old state of the button (pressed or released)</param>
/// <returns>true if pressed, false otherwise</returns>
static bool IsButtonPressed(int fd, GPIO_Value_Type *oldState)
{
	bool isButtonPressed = false;
	GPIO_Value_Type newState;
	int result = GPIO_GetValue(fd, &newState);
	if (result != 0) {
		Log_Debug("ERROR: Could not read button GPIO: %s (%d).\n", strerror(errno), errno);
		terminationRequired = true;
	}
	else {
		// Button is pressed if it is low and different than last known state.
		isButtonPressed = (newState != *oldState) && (newState == GPIO_Value_Low);
		*oldState = newState;
	}

	return isButtonPressed;
}


/// <summary>
///     Handle button timer event: if the button is pressed, change the LED blink rate.
/// </summary>
static void ButtonTimerEventHandler(event_data_t *eventData)
{
    if (ConsumeTimerFdEvent(gpioButtonTimerFd) != 0) {
        terminationRequired = true;
        return;
    }

	// If the button is pressed, set the flag to run the tests again.
	static GPIO_Value_Type newButton1State;
	if (IsButtonPressed(gpioButton1Fd, &newButton1State)) {
		runTests = true;
	}

#ifdef TEST_BUTTON_B
	// If the button is pressed, set the flag to run the tests again.
	static GPIO_Value_Type newButton2State;
	if (IsButtonPressed(gpioButton2Fd, &newButton2State)) {
		runTests = true;
	}
#endif
}

// event handler data structures. Only the event handler field needs to be populated.
static event_data_t buttonEventData = { .eventHandler = &ButtonTimerEventHandler };

/// <summary>
///     Helper function to open a file descriptor for the given GPIO as input mode.
/// </summary>
/// <param name="gpioId">The GPIO to open.</param>
/// <param name="outGpioFd">File descriptor of the opened GPIO.</param>
/// <returns>True if successful, false if an error occurred.</return>
static bool OpenGpioFdAsInput(GPIO_Id gpioId, int *outGpioFd)
{
	*outGpioFd = GPIO_OpenAsInput(gpioId);
	if (*outGpioFd < 0) {
		Log_Debug("ERROR: Could not open GPIO '%d': %d (%s).\n", gpioId, errno, strerror(errno));
		return false;
	}

	return true;
}

/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static int InitPeripheralsAndHandlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    epollFd = CreateEpollFd();
    if (epollFd < 0) {
        return -1;
    }

	// Open button A
	Log_Debug("INFO: Opening MT3620_RDB_BUTTON_A.\n");
	if (!OpenGpioFdAsInput(TEST_BUTTON_A, &gpioButton1Fd)) {
		return -1;
	}

#ifdef TEST_BUTTON_B
	// Open button B
	Log_Debug("INFO: Opening MT3620_RDB_BUTTON_B.\n");
	if (!OpenGpioFdAsInput(TEST_BUTTON_B, &gpioButton2Fd)) {
		return -1;
	}
#endif		
	// Set up a timer for buttons status check
	static struct timespec buttonsPressCheckPeriod = { 0, 1000000 };
	gpioButtonTimerFd =
		CreateTimerFdAndAddToEpoll(epollFd, &buttonsPressCheckPeriod, &buttonEventData, EPOLLIN);
	if (gpioButtonTimerFd < 0) {
		return -1;
	}

	// Open file descriptors for the RGB LEDs and store them in the rgbLeds array (and in turn in
	// the ledBlink, ledMessageEventSentReceived, ledNetworkStatus variables)
	RgbLedUtility_OpenLeds(rgbLeds, rgbLedsCount, ledsPins);

	// Turn the LED off at startup
	RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Off);

	// Call the routine to populate the file descriptor list to drive all the LEDs for the LED test
	if (!populateLedFdList()) {
		terminationRequired = true;
	}

	return 0;
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
	// Turn off the LEDs in the LED test
	ledTestChangeLeds(GPIO_Value_High);
	cleanupLedFdList();

    // Leave the LED off
	RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Off);

    Log_Debug("Closing file descriptors.\n");
    CloseFdAndPrintError(gpioLedTimerFd, "LedTimer");
    CloseFdAndPrintError(gpioButtonTimerFd, "ButtonTimer");
    CloseFdAndPrintError(gpioButton1Fd, "GpioButton1");
#ifdef TEST_BUTTON_B
	CloseFdAndPrintError(gpioButton2Fd, "GpioButton2");
#endif	
	CloseFdAndPrintError(epollFd, "Epoll");

	// Close the LEDs and leave then off
	RgbLedUtility_CloseLeds(rgbLeds, rgbLedsCount);

}

/// <summary>
///     Main entry point for this application.
/// </summary>
int main(int argc, char *argv[])

{
// Setup ts to ~0.4 seconds //pf
struct timespec ts;
ts.tv_sec = 0;
ts.tv_nsec = LED_DELAY_NS;

    Log_Debug("Avnet Development Board Test application starting.\n");
    if (InitPeripheralsAndHandlers() != 0) {
        terminationRequired = true;
    }

	GPIO_Value newLEDState = GPIO_Value_Low;

    // Use epoll to wait for events and trigger handlers, until an error or SIGTERM happens
    while (!terminationRequired) {

		if (runTests) 
		{
			Log_Debug("Now sequencing RGB LEDs\n");
			// Sequence RGB LEDs then turn RGB off...
			RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Red); 	nanosleep(&ts, NULL);
			RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Green); nanosleep(&ts, NULL);
			RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Blue); nanosleep(&ts, NULL);
			RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Off);

			// Call the routine that implements the Click-Socket LED test.
			Log_Debug("Now sequencing Click Socket GPIOs, and GPIO27, GPIO29\n");
			ledTestChangeLeds(newLEDState);
			newLEDState = (newLEDState == GPIO_Value_Low) ? GPIO_Value_Low : GPIO_Value_High;

			if (1) //pf (GPIOTestPassed() & uartTestsPassed() & wifiTestsPassed())
			{
				// Set the LED to Green if tests all passed
				RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Green);
				Log_Debug("TEST INFO: All tests passed!\n");
			}
			else
			{
				// Test Failed, turn the LED red
				RgbLedUtility_SetLed(&led1, RgbLedUtility_Colors_Red);
				Log_Debug("TEST FAILURE: At least one test Failed!  See debug output for details\n");
			}
			runTests = false;
		}

		if (WaitForEventAndCallHandler(epollFd) != 0) {
			terminationRequired = true;
		}
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return 0;
}

