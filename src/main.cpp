/**
 * @file main.cpp
 * @author Daniel Starke
 * @date 2024-03-24
 * @version 2024-03-25
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org>
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPNtpClient.h> /* https://github.com/gmag11/ESPNtpClient */
#include <TFT_eSPI.h> /* https://github.com/Bodmer/TFT_eSPI */


#ifndef ARRAY_SIZE
/**
 * Number of elements in the array.
 *
 * @param[in] x - array
 * @return element count
 */
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof(*(x)))
#endif /* ARRAY_SIZE */


/* WIFI */
#define WIFI_SSID "<your SSID>"
#define WIFI_PASS "<your WIFI password>"


/* NTP */
#define NTP_TIMEOUT 1000 /* ms */
#define NTP_SERVER "<your NTP server>"


/* TFT */
#define TFT_BACKLIGHT 4 /* pin */
#define TFT_PASS_COLOR 0x07FF /* RGB565 */
#define TFT_FAIL_COLOR 0xA082 /* RGB565 */
#define TFT_PASS_FROM "07:30"
#define TFT_PASS_TO "19:30"
static TFT_eSPI tft = TFT_eSPI();
/** TFT back light intensity table (0..255). */
static uint32_t tftBl[] {
	0,
	7,
	15,
	31,
	63,
	127,
	255
};
/** Current index into `tftBl`. */
static size_t tftBlIndex = 1;


/* Buttons */
#define BUTTON_UP 37 /* pin */
#define BUTTON_DOWN 38 /* pin */
static uint32_t bUpLast; /**< Last `millis()` when `BUTTON_UP` was triggered. */
static uint32_t bDownLast; /**< Last `millis()` when `BUTTON_DOWN` was triggered. */


/**
 * Holds a system state.
 */
struct State {
	enum { TIME_SIZE = 5 /**< Number of characters in the time string. */ };
	bool wifiOnline; /**< True if WIFI is connected in online, else false. */
	bool ntpStarted; /**< True if the NTP client has been started, else false. */
	char time[TIME_SIZE + 1]; /**< Time string used for display. */

	/**
	 * Assignment operator.
	 *
	 * @param[in] o - object to assign
	 * @return this
	 */
	State & operator= (const State & o) {
		if (this != &o) {
			this->wifiOnline = o.wifiOnline;
			this->ntpStarted = o.ntpStarted;
			memcpy(this->time, o.time, TIME_SIZE);
		}
		return *this;
	}

	/**
	 * Update stored time string from NTP client.
	 */
	void updateNtp() {
		const char * newTime = NTP.getTimeStr() ;
		memset(this->time, 0, TIME_SIZE);
		if (newTime != NULL) {
			for (size_t i = 0; i < TIME_SIZE && newTime[i] != 0; i++) {
				this->time[i] = newTime[i];
			}
		}
	}

	/**
	 * Reset all states.
	 */
	inline void resetNtp() {
		this->ntpStarted = false;
		memset(this->time, 0, TIME_SIZE + 1);
	}

	/**
	 * Checks whether the stored time string is within the given range.
	 *
	 * @param[in] from - including start time
	 * @param[in] to - including end time
	 * @return true if within, else false
	 */
	inline bool withinTimeSpan(const char * from, const char * to) const {
		return strcmp(this->time, from) >= 0 && strcmp(this->time, to) <= 0;
	}
};


/**
 * Holds the most recent system state.
 */
static State state{false, false, 0};


/**
 * Interrupt handler for the UP button.
 */
static void IRAM_ATTR buttonUpIsr() {
	const uint32_t now = millis();
	if ((now - bUpLast) < 250) {
		return;
	}
	bUpLast = now;
	tftBlIndex++;
	if (tftBlIndex >= ARRAY_SIZE(tftBl)) {
		tftBlIndex = 0;
	}
	analogWrite(TFT_BACKLIGHT, tftBl[tftBlIndex]);
}


/**
 * Interrupt handler for the DOWN button.
 */
static void IRAM_ATTR buttonDownIsr() {
	const uint32_t now = millis();
	if ((now - bDownLast) < 250) {
		return;
	}
	bDownLast = now;
	if (tftBlIndex == 0) {
		tftBlIndex = size_t(ARRAY_SIZE(tftBl) - 1);
	} else {
		tftBlIndex--;
	}
	analogWrite(TFT_BACKLIGHT, tftBl[tftBlIndex]);
}


/**
 * Initializes the system.
 */
void setup() {
	/* setup TFT */
	pinMode(TFT_BACKLIGHT, OUTPUT);
	analogWrite(TFT_BACKLIGHT, tftBl[tftBlIndex]);
	tft.init();
	tft.setRotation(1);
	tft.setTextFont(8);
	tft.fillScreen(TFT_BLACK);
	tft.setTextDatum(CC_DATUM);
	tft.setTextPadding(320);
	/* setup WIFI */
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	/* buttons */
	pinMode(BUTTON_UP, INPUT);
	pinMode(BUTTON_DOWN, INPUT);
	bUpLast = millis();
	bDownLast = millis();
	attachInterrupt(BUTTON_UP, buttonUpIsr, FALLING);
	attachInterrupt(BUTTON_DOWN, buttonDownIsr, FALLING);
}


/**
 * Updates the system state and displays the current time.
 */
void loop() {
	State newState = state;
	newState.wifiOnline = (WiFi.status() == WL_CONNECTED);
	if (state.wifiOnline && !newState.wifiOnline) {
		/* went offline -> reset time */
		newState.resetNtp();
	}
	if (newState.wifiOnline && !newState.ntpStarted) {
		/* setup NTP client */
		NTP.setNTPTimeout(NTP_TIMEOUT);
		NTP.setInterval(63);
		NTP.setTimeZone(TZ_Europe_Berlin);
		NTP.begin(NTP_SERVER);
		newState.ntpStarted = true;
	}
	if (newState.wifiOnline && newState.ntpStarted) {
		/* all up and running -> update time */
		newState.updateNtp();
	}
	if (memcmp(&state, &newState, sizeof(newState)) != 0) {
		/* state changed -> update display */
		tft.setTextColor(
			newState.withinTimeSpan(TFT_PASS_FROM, TFT_PASS_TO) ? TFT_PASS_COLOR : TFT_FAIL_COLOR,
			TFT_BLACK
		);
		tft.drawString(newState.time, 160, 120);
		state = newState;
	}
	delay(1000);
}
