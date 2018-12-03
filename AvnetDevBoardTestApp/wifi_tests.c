
// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"

#include <applibs/gpio.h>
#include <applibs/log.h>
#include <applibs/wificonfig.h>

#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "platform.h"
#include "gpio_tests.h"

// Termination state
extern sig_atomic_t terminationRequired;

const char *wifiSsid = WIFI_SSID;
const char *wifiKey = WIFI_KEY;
static bool testedOnce = false;
static bool staticTestResult;

static bool IsWiFiConnected(void) {
	WifiConfig_ConnectedNetwork network;
	int result = WifiConfig_GetCurrentNetwork(&network);
	if (result < 0) {
		return false;
	}
	else {
		return true;
	}
}
static bool DebugPrintScanFoundNetworks(void)
{
	bool returnValue = true;
	float signalLevel = NAN;

	int result = WifiConfig_TriggerScanAndGetScannedNetworkCount();
	if (result < 0) {
		Log_Debug(
			"ERROR: WifiConfig_TriggerScanAndGetScannedNetworkCount failed to get scanned "
			"network count with result %d. Errno: %s (%d).\n",
			result, strerror(errno), errno);
	}
	else if (result == 0) {
		Log_Debug("INFO: Scan found no WiFi network.\n");
	}
	else {
		size_t networkCount = (size_t)result;
		Log_Debug("INFO: Scan found %d WiFi networks:\n", result);
		WifiConfig_ScannedNetwork *networks =
			(WifiConfig_ScannedNetwork *)malloc(sizeof(WifiConfig_ScannedNetwork) * networkCount);
		result = WifiConfig_GetScannedNetworks(networks, networkCount);
		if (result < 0) {
			Log_Debug(
				"ERROR: WifiConfig_GetScannedNetworks failed to get scanned networks with "
				"result %d. Errno: %s (%d).\n",
				result, strerror(errno), errno);
		}
		else {
			// Log SSID, signal strength and frequency of the found WiFi networks
			networkCount = (size_t)result;
			for (size_t i = 0; i < networkCount; ++i) {
				Log_Debug("INFO: %3d) SSID \"%.*s\", Signal Level %d, Frequency %dMHz\n", i,
					networks[i].ssidLength, networks[i].ssid, networks[i].signalRssi,
					networks[i].frequencyMHz);

				// Check to see if we've found our SSID and updated the signalLevel variable.  If not, keep looking.  If so, move on.
				if (isnan(signalLevel)) {

					bool stringsMatch = true;
					for (int j = 0; j < networks[i].ssidLength; j++) {
						if (networks[i].ssid[j] != WIFI_SSID[j]) {
							stringsMatch = false;
						}
					}

					// We found our ssid, capture the signalRssi.
					if (stringsMatch) {
						signalLevel = (float)networks[i].signalRssi;
					}
				}
			}
		}
		
		free(networks);
	}
	
	// Check to see if our signal level is acceptable.  
	if (signalLevel < MINIMUM_WIFI_SIGNAL_STRENGTH) {
		Log_Debug("TEST FAILURE: Signal Level is below minimum!  Signal Level: %.0f, Minimum Level: %.0f\n", signalLevel, MINIMUM_WIFI_SIGNAL_STRENGTH);
		returnValue = false;
	}

	// Verify we have more than one wifi access point and an acceptable signal level for our connected wifi network.
	if ((result > 0) && returnValue) {
		return true;
	}
	else {
		return false;
	}
}

/// <summary>
///     Show details of the currently connected WiFi network.
/// </summary>
static void DebugPrintCurrentlyConnectedWiFiNetwork(void)
{
	WifiConfig_ConnectedNetwork network;
	int result = WifiConfig_GetCurrentNetwork(&network);
	if (result < 0) {
		Log_Debug("INFO: Not currently connected to a WiFi network.\n");
	}
	else {
		Log_Debug("INFO: Currently connected WiFi network: \n");
		Log_Debug("INFO: SSID \"%.*s\", BSSID %02x:%02x:%02x:%02x:%02x:%02x, Frequency %dMHz.\n",
			network.ssidLength, network.ssid, network.bssid[0], network.bssid[1],
			network.bssid[2], network.bssid[3], network.bssid[4], network.bssid[5],
			network.frequencyMHz);
	}
}

bool wifiTestsPassed(void){

	int wifiResult = 0;
	static 	bool testsResult = true;

	if (RUN_WIFI_TESTS_ONCE) {
		if (testedOnce) {
			Log_Debug("TEST INFO: Wifi tests not run again!  First pass test result was \"wifi testing %s.\"\n", testsResult ? "passed" : "failed");
			return staticTestResult;
		}
	}

	// Set the flag that says we've tested the wifi stuff once.
	testedOnce = true;
	
	wifiResult = WifiConfig_StoreWpa2Network((uint8_t*)wifiSsid, strlen(wifiSsid), wifiKey, strlen(wifiKey));

	if (wifiResult < 0) {
		if (errno == EEXIST) {
			Log_Debug("INFO: The \"%s\" WiFi network is already stored on the device.\n", wifiSsid);
		}
		else {
			Log_Debug(
				"TEST FAILURE: WifiConfig_StoreOpenNetwork failed to store WiFi network \"%s\" with "
				"result %d. Errno: %s (%d).\n",
				wifiSsid, wifiResult, strerror(errno), errno);
			testsResult = false;
		}
	}
	
	else {
		Log_Debug("TEST INFO: Successfully stored WiFi network: \"%s\".\n", wifiSsid);
	}

	// Loop for 45 seconds to try and connect to the wifi access point

	int loopCnt = 45;
	
	do {
		sleep(1);
		Log_Debug("TEST INFO: connecting to network . . .\n");
	} while (!IsWiFiConnected() & (loopCnt-- > 0));

	if (loopCnt <= 0) {
		testsResult = false;
	}
	else {
		Log_Debug("TEST INFO: Connected to network!\n");
	
		// Print the currently connected network.
		DebugPrintCurrentlyConnectedWiFiNetwork();
	}

	// Scan for networks, if we don't find any, then fail the test.
	if (!DebugPrintScanFoundNetworks()) {
		testsResult = false;
	}

	wifiResult = WifiConfig_ForgetAllNetworks();

	if (wifiResult < 0) {
		Log_Debug("ERROR: WifiConfig_ForgetAllNetworks failed to remove all stored networks. result %d. Errno: %s (%d).\n",
			wifiResult, strerror(errno), errno);
	}
	else {
		Log_Debug("TEST INFO: Successfully removed all WiFi networks\n");
	}
	
	staticTestResult = testsResult;
	return testsResult;
}

