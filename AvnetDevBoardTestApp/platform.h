#pragma once

#define UART_STRUCTS_VERSION 1

#include <applibs/gpio.h>
#include <applibs/uart.h>
#include "mt3620_rdb.h"

/*

Instructions for using the MT3620 test application

	The MT3620 test application is a highly configurable application that can be used to test MT3620 based hardware 
	designs. The basic functionality is that when the application runs it will automatically perform a set of hardware
	tests that are defined at compile time.  There is a single RGB LED that displays the testing status, any button 
	define at build time can be pressed to run the test again.

RGB test status --

	There are three states for the test and each is communicated to the operator by lighting the RGB LED different
	colors

		Tests running: White
		Test passed: Green
		Test failed: Red

Button operation --

	There can be 1 or two buttons defined at compile time.  When the application is running pressing any of these
	buttons will cause the application to run the tests a second time.  Additionally, if the "Drive GPIO/LED" test is
	enabled, pressing any button will cause the LEDs defined in the test to switch state.  So if all the LEDs are on
	when the test runs the first time, pressing any button will cause the LEDs under test to all turn off.  The button
	can be pressed as many times as necessary for the test operator to validate proper operation and record results.

	The platform.h file is used to define what tests will be executed and for each test what specific hardware will be
	tested.  There are currently 5 different test areas implemented.

1. GPIO Loopback test

-- Description

	In this test GPIO pairs are tested.  Each GPIO pair defined should be electrically connected together for the test
	to pass.  When the test runs it will operate on the GPIO pairs by setting one GPIO as an output and the second as
	an 
	input.  The test then drives the output pin through configurable transitions high/low/high and the input GPIO is
	read to validate that the correct level is present.  After the first half of the test is complete, then the
	application will reverse roles of the GPIOs and run through the test again.  So the GPIO that was an output for the
	first half of the test becomes an input and vice versa with the second GPIO.  If any of the checks fail, the test
	will fail and the debug output will provide details on which GPIO pair failed and how.

	Note that any GPIO listed need to be enabled in the app_manifest.json file.

-- Data structures

	GPIO_PAIRS gpioPairs[] = {{(GPIO_Id)26, (GPIO_Id)5}};

		The gpioPairs[] data structure is an array of GPIO pairs to test.  The example above will cause the test
		application to test GPIO-26 <--> GPIO-5.  See other examples in the platform.h file.

		As detailed above any GPIO pairs in the list should be physically connected with a jumper or other means.  If
		they are not connected the test will fail.

		If this data structure is empty, then the GPIO test portion of the application will return pass.

	GPIO_Value_Type gpioTestLevels[] = { GPIO_Value_Low, GPIO_Value_High , GPIO_Value_Low , GPIO_Value_High };

		The gpioTestLevels[] data structure is an array of GPIO levels used to exercise each GPIO pair.  In the example
		above the GPIO pair will be configured one GPIO as an output, and the second GPIO as an input.  The test
		application will then drive and validate the levels defined in the structure so in this case (Low --> High -->
		Low --> High)

		This data structure cannot be empty.

2. UART Loopback test

-- Description

	In this test UART TX and RX pins are connected together and the test application will send a short text string out
	the transmit pin and read the same message (hopefully) on the receive pin.  If the message received does not match
	the message transmitted, then the test will fail and the debug output will provide details on which UART failed.

	Note that any UARTs being tested need to be enabled in the app_manifest.json file.  Furthermore, any GPIOs
	associated with the UART under test can NOT be listed in the "Gpio": [] section of the app_manifest.json file. 
	This includes the CTS and RTS GPIOs.

-- Data structures

	UART_Id uartIDs[] = {MT3620_UART_ISU0, MT3620_UART_ISU1};

		The uartIDs[] data structure is an array of UART IDs to test.  The example above will cause the test
		application to test UART 0 and 1.

		As detailed above any UARTs defined in the uartIDs[] array must have the TX and RX pins physically connected
		with a jumper or other means; if they are not connected the test will fail.

		If this data structure is empty, then the UART test portion of the application will return pass.

3. Wifi test

-- Description

	This test validates that the device under test (DUT) can connect to a wifi access point, validates that the signal
	strength is above a configurable minimum, and that a scan of wifi access points is greater than 0.

	Note that the "WifiConfig" element in the app_manifest.json file must be set to "true" so that the application can
	modify the wifi settings in the device.

-- Data Structures

	#define WIFI_SSID "your_ssid_here"
	#define WIFI_KEY "your_ssid_key_here"
	#define MINIMUM_WIFI_SIGNAL_STRENGTH -50

		There are three #defines that the test application uses for the wifi test.  They are shown above.  The SSID and
		KEY defines are self-explanatory.  The MINIMUM_WIFI_SIGNAL_STRENGTH value is used to determine if the signal
		strength for the access point defined meets the minimum signal strength threshold.  If not, then the wifi test
		will fail and the details of the failure will be output in the debug window.

4. Drive GPIO/LED test

-- Description

	This test will simply drive all GPIOs in a list to Low (turn on LEDs) when the application runs.  If/when a button
	is pressed the LED state will toggle.  So if the LED are all on and the user presses a button, all the LEDs will
	turn off and so on.

	Note that all the GPIOs associated with the LEDs must be enabled in the app_manifest.jston file.

-- Data Structures

		GPIO_Id gpioTestList[] = { MT3620_RDB_LED2_RED, MT3620_RDB_LED2_GREEN, MT3620_RDB_LED2_BLUE};

		The gpioTestList[] data structure is used to define all the GPIOs that will be driven on/off with platform
		button presses.  The example above will cause the application to drive RGB #2 GPIOs (3) all Low when the
		application runs.  If any button is pressed, then the GPIOs defined will be driven high, and so on.

		Note that this test required an operator to visually inspect that all the LEDs defined in the data structure
		turn on/off.  The application has no way to validate operation.  To this end, the test operator can press any
		button on the platform to toggle the LEDs as many times as is needed to validate that the hardware is operating
		properly.

		If this data structure is empty, then no LEDs will be driven.

5. Test Button Information

-- Description

	The user can define one or two GPIOs connected to buttons on the platform.  The buttons will be monitored at
	runtime and if either button is pressed the application will run the test again.  If there are GPIOs listed for the
	Drive GPIO/LED test, then each button press will toggle the LED state.

-- Data Structures

	#define GPIO_USER_BUTTON1 MT3620_RDB_BUTTON_A
	#define GPIO_USER_BUTTON2 MT3620_RDB_BUTTON_B

		There are two different defines that can be used to identify what GPIO button 1 and button 2 are connected to. 
		The application will monitor one or both buttons at runtime.  If only one button is to be utilized, the
		GPIO_USER_BUTTON2 identifier should NOT be defined, but should be commented out so that at build time the
		application can correctly configure itself.
		
		Note that all the GPIOs associated with the buttons must be enabled in the app_manifest.jston file.

6. Additional build time defines

	#define RUN_WIFI_TESTS_ONCE true
	
		If we only want to run the wifi test once then set this RUN_WIFI_TESTS_ONCE to true.  If it's set to false,
		then the wifi test will run once when the application starts, but will not run again if the operator presses a
		button.  This can save test time since the wifi tests can take a few seconds.  Note that when this is set to true
		any additional tests will also report as failed and debug will state that the first wifi test failed.

		When set to false, the wifi test will run each time a button is pressed.

	#define SHOW_DEBUG

		We can turn on additional debug for troubleshooting by defining SHOW_DEBUG.

*/

// Define which development board we are building for
//#define SEEED_DEV_BOARD
#define AVNET_DEV_BOARD

#ifdef SEEED_DEV_BOARD
#ifdef AVNET_DEV_BOARD
#error "Two targets defined!  Only one target is supported"
#endif
#endif

// #define to control application functionality at build time
// Enables extra debug for troubleshooting
//#define SHOW_DEBUG

// If we only want to run the wifi test once then set this to true.  If it's set to false, then the wifi test will run
// when the application starts, but will not run again if the operator presses a button.  This can save time since the 
// wifi tests can take a few seconds.  If the test fails on the first pass and this is set to true, the test will 
// continue to report fail and output debug saying that wifi failed on the first pass.
#define RUN_WIFI_TESTS_ONCE true 

// Define how long we want to pause (in nano seconds) between lighting up LEDs in the LED test sequence.
#define LED_DELAY_NS 400000000

// Define a structure that defines the pair or GPIOs to test.
typedef struct {
	GPIO_Id gpioX;
	GPIO_Id gpioY;
} GPIO_PAIRS;

static const GPIO_Value_Type gpioTestLevels[] = { GPIO_Value_Low, GPIO_Value_High , GPIO_Value_Low , GPIO_Value_High };

// ========================>>>> Wifi test Configuration definitions <<<<==============================================

//#define WIFI_SSID "willessnetwork"
//#define WIFI_KEY "pugdawg~1"

#define WIFI_SSID "2WIRE872"
#define WIFI_KEY  "8852140819"

//#define WIFI_SSID "AVNET_LTE"
//#define WIFI_KEY  "ElliesRun"
#define MINIMUM_WIFI_SIGNAL_STRENGTH -75.0f

#ifdef SEEED_DEV_BOARD

// For the GPIO only test update the app_manifest.json file with these setting
//"Gpio": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 30, 35, 40, 41, 42, 43, 44, 56, 57, 58, 59, 60, 70, 28, 26, 29, 27, 28, 30, 66, 67, 68, 69, 33,38, 31, 36, 34, 39, 32, 37, 15, 16, 17, 18, 19, 20, 21, 22, 23, 13],
//"Uart" : [],
//"WifiConfig" : true,

// ============================>>>> Button GPIO definitions <<<<=====================================================

// Define the GPIOs that the buttons are connected to.  If there is only one button do not define TEST_BUTTONB
#define TEST_BUTTON_A MT3620_RDB_BUTTON_A
#define TEST_BUTTON_B MT3620_RDB_BUTTON_B

// =========================>>>> UART Loopback test data structures <<<<===============================================
// Set the UART List to empty.  This will bypass all UART testing
static const UART_Id uartIDs[] = {};

// =========================>>>> GPIO Loopback test data structures <<<<===============================================

// Define a structure that contains the GPIO pairs to test
static const GPIO_PAIRS gpioPairs[] = {

	// Seeed Board Header 1
	{MT3620_GPIO59, MT3620_GPIO0},
	{MT3620_GPIO56, MT3620_GPIO1},
	{MT3620_GPIO58, MT3620_GPIO2},
	{MT3620_GPIO57, MT3620_GPIO3},
	{MT3620_GPIO60, MT3620_GPIO4},

	// Seeed Board Header 2
	{(GPIO_Id)28, MT3620_GPIO30},	// Note this is a duplicate, these pins are not adjecent
	{(GPIO_Id)26, (GPIO_Id)5},
	{(GPIO_Id)29, (GPIO_Id)6},
	{(GPIO_Id)27, (GPIO_Id)7},
	{MT3620_GPIO41, MT3620_GPIO43},
	{MT3620_GPIO42, MT3620_GPIO44},

	{(GPIO_Id)66, (GPIO_Id)67},
	{(GPIO_Id)68, (GPIO_Id)69},

	//Seeed Board Header 4
	{(GPIO_Id)33, (GPIO_Id)38},
	{(GPIO_Id)31, (GPIO_Id)36},
	{(GPIO_Id)34, (GPIO_Id)39},
	{(GPIO_Id)32, (GPIO_Id)37},
	{(GPIO_Id)35, (GPIO_Id)40}
};
// =========================>>>> Drive GPIO/LED test data structures <<<<==============================================


static const GPIO_Id gpioTestList[] = { MT3620_RDB_LED2_RED, MT3620_RDB_LED2_GREEN, MT3620_RDB_LED2_BLUE,
										MT3620_RDB_LED3_RED, MT3620_RDB_LED3_GREEN, MT3620_RDB_LED3_BLUE,
										MT3620_RDB_LED4_RED, MT3620_RDB_LED4_GREEN, MT3620_RDB_LED4_BLUE };

// GPIOs to add to the app_manifest.json file for the LED test
// 15, 16, 17, 18, 19, 20, 21, 22, 23


// =============================>>>> RGB LED test Status defines <<<<==================================================

// Define the GPIOs for the RGB user LED
#define GPIO_RED MT3620_RDB_LED1_RED
#define GPIO_GREEN MT3620_RDB_LED1_GREEN
#define GPIO_BLUE MT3620_RDB_LED1_BLUE

// ================================>>>> Test button defines <<<<=======================================================

// Define the GPIO for the user button
#define GPIO_USER_BUTTON1 MT3620_RDB_BUTTON_A
#define GPIO_USER_BUTTON2 MT3620_RDB_BUTTON_B

#endif


#ifdef AVNET_DEV_BOARD

// GPIO26-30 are ISU0 - first 4 GPIOs are UART0
#define MT3620_GPIO26 ((GPIO_Id)26)
#define MT3620_GPIO27 ((GPIO_Id)27)
#define MT3620_GPIO28 ((GPIO_Id)28)
#define MT3620_GPIO29 ((GPIO_Id)29)

// GPIO31-35 are ISU1 - first 4 GPIOs are UART1
#define MT3620_GPIO31 ((GPIO_Id)31)
#define MT3620_GPIO32 ((GPIO_Id)32)
#define MT3620_GPIO33 ((GPIO_Id)33)
#define MT3620_GPIO34 ((GPIO_Id)34)

// GPIO36-40 are ISU2 - first 4 GPIOs are UART2
#define MT3620_GPIO37 ((GPIO_Id)37)
#define MT3620_GPIO38 ((GPIO_Id)38)

// For the GPIO only test update the app_manifest.json file with these setting
//"Gpio": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 30, 35, 40, 41, 42, 43, 44, 56, 57, 58, 59, 60, 70, 28, 26, 29, 27, 28, 30, 66, 67, 68, 69, 33,38, 31, 36, 34, 39, 32, 37, 15, 16, 17, 18, 19, 20, 21, 22, 23, 13],
//"Uart" : [],
//"WifiConfig" : true,

// ============================>>>> Button GPIO definitions <<<<=====================================================

// Define the GPIOs that the buttons are connected to.  If there is only one button do not define TEST_BUTTONB
#define TEST_BUTTON_A MT3620_RDB_BUTTON_A
#define TEST_BUTTON_B MT3620_RDB_BUTTON_B

// =========================>>>> UART Loopback test data structures <<<<===============================================
// Set the UART List to empty.  This will bypass all UART testing
static const UART_Id uartIDs[] = {};

// =========================>>>> GPIO Loopback test data structures <<<<===============================================

// Define a structure that contains the GPIO pairs to test

//	[8, 9, 10, 12,   42, 16, 34, 31, 33, 32, 0, 2, 28, 26, 37, 38,   43, 17, 35, 31, 33, 32, 1, 2, 28, 26, 37, 38,   27, 29],

static const GPIO_PAIRS gpioPairs[] = {};
/*
// Seeed Board Header 1
	{MT3620_GPIO59, MT3620_GPIO0},
	{MT3620_GPIO56, MT3620_GPIO1},
	{MT3620_GPIO58, MT3620_GPIO2},
	{MT3620_GPIO57, MT3620_GPIO3},
	{MT3620_GPIO60, MT3620_GPIO4},

	// Seeed Board Header 2
	{(GPIO_Id)28, MT3620_GPIO30},	// Note this is a duplicate, these pins are not adjecent
	{(GPIO_Id)26, (GPIO_Id)5},
	{(GPIO_Id)29, (GPIO_Id)6},
	{(GPIO_Id)27, (GPIO_Id)7},
	{MT3620_GPIO41, MT3620_GPIO43},
	{MT3620_GPIO42, MT3620_GPIO44},

	{(GPIO_Id)66, (GPIO_Id)67},
	{(GPIO_Id)68, (GPIO_Id)69},

	//Seeed Board Header 4
	{(GPIO_Id)33, (GPIO_Id)38},
	{(GPIO_Id)31, (GPIO_Id)36},
	{(GPIO_Id)34, (GPIO_Id)39},
	{(GPIO_Id)32, (GPIO_Id)37},
	{(GPIO_Id)35, (GPIO_Id)40}
};
*/
// =========================>>>> Drive GPIO/LED test data structures <<<<==============================================

//	[4, 5, 8, 9, 10,   42, 16, 34, 31, 33, 32, 0, 2, 28, 26, 37, 38,   43, 17, 35, 31, 33, 32, 1, 2, 28, 26, 37, 38,   27, 29]

static const GPIO_Id gpioTestList[] = { MT3620_GPIO42, MT3620_GPIO16, MT3620_GPIO34, MT3620_GPIO31, MT3620_GPIO33, MT3620_GPIO32, MT3620_GPIO0, MT3620_GPIO2, MT3620_GPIO28, MT3620_GPIO26, MT3620_GPIO37, MT3620_GPIO38,
										MT3620_GPIO43, MT3620_GPIO17, MT3620_GPIO35, MT3620_GPIO1,  MT3620_GPIO4,  MT3620_GPIO5,  MT3620_GPIO27, MT3620_GPIO29 };

static const GPIO_Id LedSeqList[] = { MT3620_GPIO42, MT3620_GPIO16, MT3620_GPIO34, MT3620_GPIO31, MT3620_GPIO33, MT3620_GPIO32, MT3620_GPIO0, MT3620_GPIO2, MT3620_GPIO28, MT3620_GPIO26, MT3620_GPIO37, MT3620_GPIO38,
									  MT3620_GPIO43, MT3620_GPIO17, MT3620_GPIO35, MT3620_GPIO31, MT3620_GPIO33, MT3620_GPIO32, MT3620_GPIO1, MT3620_GPIO2, MT3620_GPIO28, MT3620_GPIO26, MT3620_GPIO37, MT3620_GPIO38,
									  MT3620_GPIO4,  MT3620_GPIO5,  MT3620_GPIO27, MT3620_GPIO29 };

// GPIOs to add to the app_manifest.json file for the LED test
// 15, 16, 17, 18, 19, 20, 21, 22, 23

// =============================>>>> RGB LED test Status defines <<<<==================================================

// Define the GPIOs for the RGB user LED
#define GPIO_RED MT3620_RDB_LED1_RED
#define GPIO_GREEN MT3620_RDB_LED1_GREEN
#define GPIO_BLUE MT3620_RDB_LED1_BLUE

// ================================>>>> Test button defines <<<<=======================================================

// Define the GPIO for the user button
#define GPIO_USER_BUTTON1 MT3620_RDB_BUTTON_A
#define GPIO_USER_BUTTON2 MT3620_RDB_BUTTON_B

#endif
