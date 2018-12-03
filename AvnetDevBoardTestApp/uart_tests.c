
//#include <applibs/gpio.h>
#include <applibs/log.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform.h"
#include "uart_tests.h"

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"


// Termination state
extern sig_atomic_t terminationRequired;

//bool testUART(UART_Id);
//static void SendUartMessage(int, const char*);
//bool stringsMatch(char*, const char*, int);

// Determine how many UARTs we have to test
static const int numUARTs = sizeof(uartIDs) / sizeof(GPIO_Id);

/// <summary>
///     Helper function to send a fixed message via the given UART.
/// </summary>
/// <param name="uartFd">The open file descriptor of the UART to write to</param>
/// <param name="dataToSend">The data to send over the UART</param>
static void SendUartMessage(int uartFd, const char *dataToSend)
{
	size_t totalBytesSent = 0;
	size_t totalBytesToSend = strlen(dataToSend);
	int sendIterations = 0;
	while (totalBytesSent < totalBytesToSend) {
		sendIterations++;

		// Send as much of the remaining data as possible
		size_t bytesLeftToSend = totalBytesToSend - totalBytesSent;
		const char *remainingMessageToSend = dataToSend + totalBytesSent;
		ssize_t bytesSent = write(uartFd, remainingMessageToSend, bytesLeftToSend);
		if (bytesSent < 0) {
			Log_Debug("ERROR: Could not write to UART: %s (%d).\n", strerror(errno), errno);
			terminationRequired = true;
			return;
		}

		totalBytesSent += (size_t)bytesSent;
	}

	//	Log_Debug("INFO: Sent %zu bytes over UART in %d calls\n", totalBytesSent, sendIterations);
}

bool stringsMatch(char* string1, const  char* string2, int len) {

	for (int i = 0; i < len; i++) {
		if ((char)string1[i] != (char)string2[i]) {
			Log_Debug("TEST FAILURE: UART compareString() Failed\n");
			return false;
		}
	}
	return true;
}


bool testUART(UART_Id uartId) {

#define RECEIVE_BUFFER_SIZE 128

	///<summary>static receive buffer for UART</summary>

	static int uartFd = -1;

	bool returnValue = true;

	static char receiveBuffer[RECEIVE_BUFFER_SIZE];
	memset(&receiveBuffer, 0, sizeof(receiveBuffer));
	char *pchSegment = &receiveBuffer[0];

	const char* testString = "Testing, Testing, 1, 2, 3";

	// Create a UART_Config object, open the UART and set up UART event handler
	UART_Config uartConfig;
	UART_InitConfig(&uartConfig);
	uartConfig.baudRate = 9600;
	uartConfig.flowControl = UART_FlowControl_None;
	uartFd = UART_Open(uartId, &uartConfig);
	if (uartFd < 0) {
		Log_Debug("ERROR: Could not open UART: %s (%d).\n", strerror(errno), errno);
		return -1;
	}

	// Send the canned message over the uart
	SendUartMessage(uartFd, testString);

	// Setup ts to ~0.02 seconds to allow time for the UART message to be received
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 22222222;
	nanosleep(&ts, NULL);

	// Poll the UART and store the byte(s)
	ssize_t nBytesRead = read(uartFd, (void *)pchSegment, RECEIVE_BUFFER_SIZE);

	if (nBytesRead < 0) {
		Log_Debug("ERROR: Could not read UART: %s (%d)\n", strerror(errno), errno);
		terminationRequired = true;
		return false;
	}
	else {
		if (nBytesRead == 0 || !stringsMatch(pchSegment, testString, nBytesRead)) {
			Log_Debug("TEST FAILURE: UART test failed on ISU%d\n", uartId - 3);
			returnValue = false;
		}
		else {
#ifdef SHOW_DEBUG
			Log_Debug("TEST INFO: Uart test passed for ISU%d\n", uartId - 3);
#endif
		}
	}

	memset(&receiveBuffer, 0, sizeof(receiveBuffer));
	CloseFdAndPrintError(uartFd, "Uart");
	return returnValue;
}


bool uartTestsPassed(void) {

	bool testsPassed = true;

	//  If there are no uarts defined, then simply return a pass result.
	if (numUARTs == 0) {
		return testsPassed;
	}

	for (int i = 0; i < numUARTs; i++) {

		if (!testUART((uartIDs[i]))) {
			testsPassed = false;
		}
	}

	return testsPassed;

}

