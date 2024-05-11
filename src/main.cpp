/**
 * @file main.cpp
 * @author Daniel Starke
 * @date 2024-03-24
 * @version 2024-05-11
 *
 * Copyright (c) 2024 Daniel Starke
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPNtpClient.h> /* https://github.com/gmag11/ESPNtpClient */
#include <TFT_eSPI.h> /* https://github.com/Bodmer/TFT_eSPI */
#include <AsyncTCP.h> /* https://github.com/mathieucarbou/AsyncTCP */
#include <ESPAsyncWebServer.h> /* https://github.com/mathieucarbou/ESPAsyncWebServer */
#include "IniParser.hpp"
#include "SvgData.hpp"

extern "C" {
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
} /* C */


#ifndef ARRAY_SIZE
/**
 * Number of elements in the array.
 *
 * @param[in] x - array
 * @return element count
 */
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof(*(x)))
#endif /* ARRAY_SIZE */


/** Configuration file path. */
#define CONFIG_FILE "/config.ini"


/** Global mutex. */
SemaphoreHandle_t mutex;


/* TFT */
#define TFT_BACKLIGHT 4 /* pin */
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
/** Parsed SVG data for the analog clock. */
static NSVGimage * svgImg = NULL;
/** SVG image rasterizer instance. */
static NSVGrasterizer * svgRast = NULL;
/** Rendered SVG data for the analog clock in RGBA32. */
static unsigned char * imgBuf = NULL;
/** Initial paths of the hour clock hand. */
static NSVGpath * svgPathsHour = NULL;
/** Initial paths of the minute clock hand. */
static NSVGpath * svgPathsMin = NULL;

/* Buttons */
#define BUTTON_UP 37 /* pin */
#define BUTTON_DOWN 38 /* pin */
static uint32_t bUpLast; /**< Last `millis()` when `BUTTON_UP` was triggered. */
static uint32_t bDownLast; /**< Last `millis()` when `BUTTON_DOWN` was triggered. */


/**
 * Holds the system configuration.
 */
struct Config {
	enum {
		MAX_STRING = 255, /**< Maximum number of characters in a general string. */
		HOST_SIZE = 63, /**< Maximum number of characters in a host name string. */
		TIME_SIZE = 5, /**< Number of characters in a time string. */
		TYPE_SIZE = 8 /**< Number of characters in a type string. */
	};
	char wifiSsid[MAX_STRING + 1]; /**< WIFI SSID to connect to. */
	char wifiPass[MAX_STRING + 1]; /**< WIFI password to use. */
	char mdnsHost[HOST_SIZE + 1]; /**< Multicast DNS host name to publish within domain `.local`. */
	char otaPass[MAX_STRING + 1]; /**< Over-the-Air updater password. */
	uint32_t ntpTimeout; /**< NTP request timeout in milliseconds. */
	char ntpServer[MAX_STRING + 1]; /**< NTP server address as host name or IPv4 address. */
	uint32_t clockPassColor; /**< Passing clock color in RGB565. */
	uint32_t clockFailColor; /**< Failing clock color in RGB565. */
	char clockPassFrom[TIME_SIZE + 1]; /**< Starting time (inclusive) in HH:SS to use the passing color. */
	char clockPassTo[TIME_SIZE + 1]; /**< Endinding time (exclusive) in HH:SS to use the passing color. */
	char clockType[TYPE_SIZE + 1]; /**< Either "digital" or "analog". */

	/**
	 * Checks if the Multicast DNS host name is valid.
	 *
	 * @return true if valid, else false
	 */
	inline bool checkMdnsHost() const noexcept {
		/* ^[a-zA-Z]([0-9a-zA-Z-]{0,61}[0-9a-zA-Z])?$ */
		const char * str = Config::checkDomainLabel(this->mdnsHost);
		if (str == NULL || *str != 0) {
			return false;
		}
		return true;
	}

	/**
	 * Checks if the NTP server address is valid.
	 *
	 * @return true if valid, else false
	 */
	bool checkNtpServer() const noexcept {
		/* ^(([a-zA-Z]([0-9a-zA-Z-]{0,61}[0-9a-zA-Z])?(\.[a-zA-Z]([0-9a-zA-Z-]{0,61}[0-9a-zA-Z])?)*)|(((25[0-5]|(2[0-4]|1\d|[1-9]|)\d)\.?\b){4}))$ */
		if ( Config::checkIpv4(this->ntpServer) ) {
			return true;
		}
		const char * str = this->ntpServer;
		do {
			str = Config::checkDomainLabel(str);
		} while (str != NULL && *str == '.');
		if (str == NULL || *str != 0) {
			return false;
		}
		return true;
	}

	/**
	 * Checks if the starting passing time is valid.
	 *
	 * @return true if valid, else false
	 */
	inline bool checkClockPassFrom() const noexcept {
		return Config::checkTime(this->clockPassFrom);
	}

	/**
	 * Checks if the ending passing time is valid.
	 *
	 * @return true if valid, else false
	 */
	inline bool checkClockPassTo() const noexcept {
		return Config::checkTime(this->clockPassTo);
	}

	/**
	 * Checks if the clock type is valid.
	 *
	 * @return true if valid, else false
	 */
	inline bool checkClockType() const noexcept {
		return strcmp(this->clockType, "digital") == 0 || strcmp(this->clockType, "analog") == 0;
	}

	/**
	 * Loads the system configuration from the internal flash.
	 * See `CONFIG_FILE`.
	 *
	 * @return true on success, else false
	 */
	bool load() {
		static Config tmp;
		static const char * params[] = {
			"WIFI.SSID",
			"WIFI.PASS",
			"MDNS.HOST",
			"OTA.PASS",
			"NTP.TIMEOUT",
			"NTP.SERVER",
			"CLOCK.PASS_COLOR",
			"CLOCK.FAIL_COLOR",
			"CLOCK.PASS_FROM",
			"CLOCK.PASS_TO",
			"CLOCK.TYPE"
		};
		enum {
			HAS_WIFI_SSID,
			HAS_WIFI_PASS,
			HAS_MDNS_HOST,
			HAS_OTA_PASS,
			HAS_NTP_TIMEOUT,
			HAS_NTP_SERVER,
			HAS_CLOCK_PASS_COLOR,
			HAS_CLOCK_FAIL_COLOR,
			HAS_CLOCK_PASS_FROM,
			HAS_CLOCK_PASS_TO,
			HAS_CLOCK_TYPE,
			HAS_ALL
		};
		File file = LittleFS.open(CONFIG_FILE, FILE_READ);
		if ( ! file ) {
			return false;
		}
		memset(&tmp, 0, sizeof(tmp));
		uint32_t found = 0;
		const auto fileReader = [&] () -> int {
			return file.read();
		};
		bool (Config::* checker)() const noexcept = NULL;
		const auto valueMapper = [&] (IniParser::Context & ctx, const bool parsed) -> bool {
			checker = NULL;
			if ( ! parsed ) {
				if (ctx.group == "WIFI") {
					if (ctx.key == "SSID") {
						found |= uint32_t(1) << HAS_WIFI_SSID;
						ctx.mapString(tmp.wifiSsid);
					} else if (ctx.key == "PASS") {
						found |= uint32_t(1) << HAS_WIFI_PASS;
						ctx.mapString(tmp.wifiPass);
					}
				} else if (ctx.group == "MDNS") {
					if (ctx.key == "HOST") {
						found |= uint32_t(1) << HAS_MDNS_HOST;
						ctx.mapString(tmp.mdnsHost);
						checker = &Config::checkMdnsHost;
					}
				} else if (ctx.group == "OTA") {
					if (ctx.key == "PASS") {
						found |= uint32_t(1) << HAS_OTA_PASS;
						ctx.mapString(tmp.otaPass);
					}
				} else if (ctx.group == "NTP") {
					if (ctx.key == "TIMEOUT") {
						found |= uint32_t(1) << HAS_NTP_TIMEOUT;
						ctx.mapNumber(tmp.ntpTimeout, 0, 0xFFFF);
					} else if (ctx.key == "SERVER") {
						found |= uint32_t(1) << HAS_NTP_SERVER;
						ctx.mapString(tmp.ntpServer);
						checker = &Config::checkNtpServer;
					}
				} else if (ctx.group == "CLOCK") {
					if (ctx.key == "PASS_COLOR") {
						found |= uint32_t(1) << HAS_CLOCK_PASS_COLOR;
						ctx.mapNumber(tmp.clockPassColor, 0, 0xFFFF);
					} else if (ctx.key == "FAIL_COLOR") {
						found |= uint32_t(1) << HAS_CLOCK_FAIL_COLOR;
						ctx.mapNumber(tmp.clockFailColor, 0, 0xFFFF);
					} else if (ctx.key == "PASS_FROM") {
						found |= uint32_t(1) << HAS_CLOCK_PASS_FROM;
						ctx.mapString(tmp.clockPassFrom);
						checker = &Config::checkClockPassFrom;
					} else if (ctx.key == "PASS_TO") {
						found |= uint32_t(1) << HAS_CLOCK_PASS_TO;
						ctx.mapString(tmp.clockPassTo);
						checker = &Config::checkClockPassTo;
					} else if (ctx.key == "TYPE") {
						found |= uint32_t(1) << HAS_CLOCK_TYPE;
						ctx.mapString(tmp.clockType);
						checker = &Config::checkClockType;
					}
				}
			} else if (checker != NULL && ((*this).*checker)() == false) {
				return false;
			}
			return true; /* ignore unknown keys */
		};
		const size_t res = iniParseFn<16>(fileReader, valueMapper);
		file.close();
		const bool foundAll = found == uint32_t((uint32_t(1) << HAS_ALL) - 1);
		if (res != 0) {
			log_e("Syntax error in system configuration at line %u.", unsigned(res));
			return false;
		} else if ( ! foundAll ) {
			log_e("Missing configuration keys:");
			for (size_t i = 0; i < HAS_ALL; i++) {
				if ((found & (uint32_t(1) << i)) == 0) {
					log_e(" - %s", params[i]);
				}
			}
			return false;
		}
		/* all checks passed -> update to new configuration */
		*this = tmp;
		return true;
	}

	/**
	 * Stores the system configuration on the internal flash.
	 * See `CONFIG_FILE`.
	 *
	 * @return true on success, else false
	 */
	bool store() {
		static const char * fmt = R"([WIFI]

SSID = "%s"
PASS = "%s"

[MDNS]

# host name for domain .local
HOST = "%s"

[OTA]

PASS = "%s"

[NTP]

# milliseconds
TIMEOUT = %lu
# host name or IPv4 address
SERVER = "%s"

[CLOCK]

# RGB565
PASS_COLOR = 0x%04X
# RGB565
FAIL_COLOR = 0x%04X
# HH:MM
PASS_FROM = "%s"
# HH:MM
PASS_TO = "%s"
# digital, analog
TYPE = "%s"
)";
		File file = LittleFS.open(CONFIG_FILE, FILE_WRITE, true);
		if ( ! file ) {
			return false;
		}
		/* write configuration file as INI file */
		if (file.printf(fmt,
			this->wifiSsid,
			this->wifiPass,
			this->mdnsHost,
			this->otaPass,
			this->ntpTimeout,
			this->ntpServer,
			this->clockPassColor,
			this->clockFailColor,
			this->clockPassFrom,
			this->clockPassTo,
			this->clockType
		) <= 0) {
			file.close();
			return false;
		}
		file.close();
		return true;
	}
private:
	/**
	 * Checks if the given string is a valid domain name label according to RFC1035 Ch. 2.3.1.
	 * A label may be terminated by a null-terminator or dot.
	 *
	 * @param[in] str - string to check
	 * @return end of the label if valid, else NULL
	 */
	static const char * checkDomainLabel(const char * str) noexcept {
		/* ^[a-zA-Z]([0-9a-zA-Z-]{0,61}[0-9a-zA-Z])?(\.|$) */
		if ( ! isalpha(*str) ) {
			return NULL;
		}
		str++;
		const char * first = str;
		for (unsigned int i = 0; i < 62; i++) {
			if (*str == '.' || *str == 0) {
				break; /* end of label */
			}
			if (*str != '-' && ( ! isalnum(*str) )) {
				return NULL;
			}
			str++;
		}
		if ((str - first) > 62 || str[-1] == '-') {
			return NULL;
		}
		return str;
	}

	/**
	 * Checks if the given string is a valid time in the format `HH:MM`.
	 *
	 * @param[in] str - string to check
	 * @return if valid, else false
	 */
	static bool checkTime(const char * str) noexcept {
		if (!isdigit(str[0]) || !isdigit(str[1]) || str[2] != ':'
			|| !isdigit(str[3]) || !isdigit(str[4]) || str[5] != 0) {
			return false;
		}
		const uint32_t hour = uint32_t(((str[0] - '0') * 10) + str[1] - '0');
		if (hour > 23) {
			return false;
		}
		const uint32_t minute = uint32_t(((str[3] - '0') * 10) + str[4] - '0');
		if (minute > 59) {
			return false;
		}
		return true;
	}

	/**
	 * Checks if the given string is a valid IPv4 address.
	 *
	 * @param[in] str - string to check
	 * @return if valid, else false
	 */
	static bool checkIpv4(const char * str) noexcept {
		for (unsigned int i = 0; i < 4; i++) {
			if (i != 0) {
				if (*str != '.') {
					return false;
				}
				str++;
			}
			unsigned int part = 0;
			while ( isdigit(*str) ) {
				part = (part * 10) + (*str - '0');
				if (part > 255) {
					return false;
				}
				str++;
			}
		}
		return *str == 0;
	}
};


/**
 * Holds the current system configuration.
 */
static Config config{{0}, {0}, {0}, {0}, 0, {0}, 0, 0, {0}, {0}, {0}};


/**
 * Holds a system state.
 */
struct State {
	bool wifiOnline; /**< True if WIFI is connected in online, else false. */
	bool otaStarted; /**< True if the Over-the-Air updater has been started, else false. */
	bool ntpStarted; /**< True if the NTP client has been started, else false. */
	bool clockChanged; /**< True if the clock type has been changed, else false. */
	bool configChanged; /**< True if the configuration has been changed and needs to be stored, else false. */
	char time[Config::TIME_SIZE + 1]; /**< Time string used for display. */

	/**
	 * Assignment operator.
	 *
	 * @param[in] o - object to assign
	 * @return this
	 */
	State & operator= (const State & o) noexcept {
		if (this != &o) {
			this->wifiOnline = o.wifiOnline;
			this->otaStarted = o.otaStarted;
			this->ntpStarted = o.ntpStarted;
			this->clockChanged = o.clockChanged;
			this->configChanged = o.configChanged;
			memcpy(this->time, o.time, Config::TIME_SIZE);
		}
		return *this;
	}

	/**
	 * Update stored time string from NTP client.
	 */
	void updateNtp() noexcept {
		const char * newTime = NTP.getTimeStr() ;
		memset(this->time, 0, Config::TIME_SIZE);
		if (newTime != NULL) {
			for (size_t i = 0; i < Config::TIME_SIZE && newTime[i] != 0; i++) {
				this->time[i] = newTime[i];
			}
		}
	}

	/**
	 * Reset online states.
	 */
	inline void setOffline() {
		this->otaStarted = false;
		ArduinoOTA.end();
		this->ntpStarted = false;
		memset(this->time, 0, Config::TIME_SIZE + 1);
	}

	/**
	 * Checks whether the stored time string is within the given range.
	 *
	 * @param[in] from - including start time
	 * @param[in] to - excluding end time
	 * @return true if within, else false
	 */
	inline bool withinTimeSpan(const char * from, const char * to) const noexcept {
		return strcmp(this->time, from) >= 0 && strcmp(this->time, to) < 0;
	}
};


/**
 * Holds the most recent system state.
 */
static State state{false, false, false, false, false, false, 0, 0};


/* web server */
AsyncWebServer server(80);


/**
 * Converts an RGB565 value to an RGB24 value.
 *
 * @param[in] val - RGB565 value
 * @return RGB24 value
 */
static inline uint32_t fromRgb565(const uint32_t val) noexcept {
	const uint32_t r = (val << 8) & 0xF80000UL;
	const uint32_t g = (val << 5) & 0xFC00UL;
	const uint32_t b = (val << 3) & 0xF8UL;
	return r | g | b;
}


/**
 * Interrupt handler for the UP button.
 */
static void IRAM_ATTR buttonUpIsr() noexcept {
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
static void IRAM_ATTR buttonDownIsr() noexcept {
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
 * Returns the SVG shape with the given ID
 *
 * @param[in] id - shape ID
 * @return shape or `NULL` if not found
 */
static NSVGshape * svgGetShape(const char * id) noexcept {
	NSVGshape * shape;
	for (shape = svgImg->shapes; shape != NULL; shape = shape->next) {
		if (strcmp(shape->id, id) == 0) {
			break;
		}
	}
	return shape;
}


/**
 * Creates a copy of all paths from the SVG shape of the given ID.
 *
 * @param[in] id - shape ID
 * @return allocated paths or `NULL` on allocation error
 */
static NSVGpath * svgDuplicateShapePaths(const char * id) noexcept {
	NSVGshape * shape = svgGetShape(id);
	if (shape == NULL) {
		return NULL; /* ID not found */
	}
	NSVGpath * res = NULL, * ptr = NULL;
	for (NSVGpath * path = shape->paths; path != NULL; path = path->next) {
		if (res == NULL) {
			res = nsvgDuplicatePath(path);
			ptr = res;
		} else {
			ptr->next = nsvgDuplicatePath(path);
		}
		if (ptr == NULL) {
			goto onError;
		}
		ptr = ptr->next;
	}
	return res;
onError:
	nsvg__deletePaths(res);
	return NULL;
}


/**
 * Sets the paths for the SVG shape with the given ID to the
 * passed path list. Both lists much match in size.
 *
 * @param[in] id - shape ID
 * @param[in] p - path list
 * @return true on success, else false
 */
static bool svgSetPaths(const char * id, NSVGpath * p) noexcept {
	NSVGshape * shape = svgGetShape(id);
	if (shape == NULL) {
		return false; /* ID not found */
	}
	for (NSVGpath * path = shape->paths; path != NULL; path = path->next) {
		if (p == NULL || p->npts != path->npts) {
			return false;
		}
		memcpy(path->pts, p->pts, p->npts * sizeof(float) * 2);
		memcpy(path->bounds, p->bounds, sizeof(p->bounds));
		p = p->next;
	}
	return p == NULL;
}


/**
 * Converts an RGB565 value to an SVG RGBA32 value.
 *
 * @param[in] val - RGB565 value
 * @return SVG RGBA32 value
 */
static inline uint32_t svgFromRgb565(const uint32_t val) noexcept {
	const uint32_t r = (val >> 8) & 0xF8UL;
	const uint32_t g = (val << 5) & 0xFC00UL;
	const uint32_t b = (val << 19) & 0xF80000UL;
	const uint32_t a = 0xFF000000UL;
	return r | g | b | a;
}


/**
 * Sets the paths for the SVG shape with the given ID to the
 * passed path list. Both lists much match in size.
 *
 * @param[in] id - shape ID
 * @param[in] p - path list
 * @return true on success, else false
 */
static bool svgSetFill(const char * id, const uint32_t color) {
	NSVGshape * shape = svgGetShape(id);
	if (shape == NULL) {
		return false; /* ID not found */
	}
	if (shape->fill.type != NSVG_PAINT_COLOR) {
		return false; /* not a simple color fill */
	}
	shape->fill.color = static_cast<unsigned int>(color);
	return true;
}


/**
 * Transforms the SVG shape of the given ID by the passed angle
 * around the center (160x120).
 *
 * @param[in] id - shape ID
 * @param[in] angle - angle in degrees
 */
static bool svgRotateShape(const char * id, float angle) noexcept {
	NSVGshape * shape = svgGetShape(id);
	if (shape == NULL) {
		return false; /* ID not found */
	}
	/* calculate root transformation matrix */
	float m[6];
	float t[6];
	NSVGpath * path;
	float bounds[4];
	float * curve;
	int i;
	nsvg__xformIdentity(m);
	nsvg__xformSetTranslation(t, -160.0f, -120.0f);
	nsvg__xformMultiply(m, t);
	nsvg__xformSetRotation(t, angle / 180.0f * NSVG_PI);
	nsvg__xformMultiply(m, t);
	nsvg__xformSetTranslation(t, 160.0f, 120.0f);
	nsvg__xformMultiply(m, t);
	/* recalculate shape bounds */
	for (path = shape->paths; path != NULL; path = path->next) {
		/* apply transformation to paths */
		for (i = 0; i < path->npts; i++) {
			nsvg__xformPoint(&path->pts[i * 2], &path->pts[(i * 2) + 1], path->pts[i * 2], path->pts[(i * 2) + 1], m);
		}
		for (i = 0; i < (path->npts - 1); i += 3) {
			curve = &path->pts[i * 2];
			nsvg__curveBounds(bounds, curve);
			if (i == 0) {
				path->bounds[0] = bounds[0];
				path->bounds[1] = bounds[1];
				path->bounds[2] = bounds[2];
				path->bounds[3] = bounds[3];
			} else {
				path->bounds[0] = nsvg__minf(path->bounds[0], bounds[0]);
				path->bounds[1] = nsvg__minf(path->bounds[1], bounds[1]);
				path->bounds[2] = nsvg__maxf(path->bounds[2], bounds[2]);
				path->bounds[3] = nsvg__maxf(path->bounds[3], bounds[3]);
			}
		}
		if (path == shape->paths) {
			shape->bounds[0] = shape->paths->bounds[0];
			shape->bounds[1] = shape->paths->bounds[1];
			shape->bounds[2] = shape->paths->bounds[2];
			shape->bounds[3] = shape->paths->bounds[3];
		} else {
			shape->bounds[0] = nsvg__minf(shape->bounds[0], path->bounds[0]);
			shape->bounds[1] = nsvg__minf(shape->bounds[1], path->bounds[1]);
			shape->bounds[2] = nsvg__maxf(shape->bounds[2], path->bounds[2]);
			shape->bounds[3] = nsvg__maxf(shape->bounds[3], path->bounds[3]);
		}
	}
	return true;
}


/**
 * Initializes the system.
 */
void setup() {
	mutex = xSemaphoreCreateBinary();
	xSemaphoreGive(mutex);
	/* mount flash file system */
	if ( ! LittleFS.begin(false, "/root", 10, "root") ) {
		log_e("Failed to mount flash file system.");
		esp_deep_sleep_start();
	}
	/* load system configuration */
	if ( ! config.load() ) {
		log_e("Failed to load system configuration.");
		esp_deep_sleep_start();
	}
	/* setup TFT */
	pinMode(TFT_BACKLIGHT, OUTPUT);
	analogWrite(TFT_BACKLIGHT, tftBl[tftBlIndex]);
	tft.init();
	tft.setRotation(1);
	tft.setTextFont(8);
	tft.fillScreen(TFT_BLACK);
	tft.setTextDatum(CC_DATUM);
	tft.setTextPadding(320);
	tft.setSwapBytes(true);
	/* initialize analog clock */
	char * svgStr = strdup(svgData);
	if (svgStr == NULL) {
		log_e("Failed to create string from SVG data.");
		esp_deep_sleep_start();
	}
	svgImg = nsvgParse(svgStr, "px", 96);
	free(svgStr);
	if (svgImg == NULL) {
		log_e("Failed to parse analog clock data.");
		esp_deep_sleep_start();
	}
	svgPathsHour = svgDuplicateShapePaths("hour");
	if (svgPathsHour == NULL) {
		log_e("Failed to copy SVG paths for hour clock hand.");
		esp_deep_sleep_start();
	}
	svgPathsMin = svgDuplicateShapePaths("min");
	if (svgPathsMin == NULL) {
		log_e("Failed to copy SVG paths for minutes clock hand.");
		esp_deep_sleep_start();
	}
	svgRast = nsvgCreateRasterizer();
	if (svgRast == NULL) {
		log_e("Memory exhausted while trying to allocate SVG rasterizer instance.");
		esp_deep_sleep_start();
	}
	imgBuf = static_cast<unsigned char *>(malloc(320 * 240 * 4));
	if (imgBuf == NULL) {
		log_e("Memory exhausted while trying to allocate analog clock image buffer.");
		esp_deep_sleep_start();
	}
	/* setup WIFI */
	WiFi.mode(WIFI_STA);
	WiFi.begin(config.wifiSsid, config.wifiPass);
	/* setup Over-the-Air updater */
	ArduinoOTA.setHostname(config.mdnsHost);
	ArduinoOTA.setPassword(config.otaPass);
	/* web server */
	server.on("/", HTTP_GET, [] (AsyncWebServerRequest * request) {
		/* do not use server.serveStatic("/", LittleFS, "/web/index.html") to avoid access to ../config.ini */
		if (xSemaphoreTake(mutex, TickType_t(100)) == pdTRUE) {
			request->send(LittleFS, "/web/index.html");
			xSemaphoreGive(mutex);
		} else {
			request->send(503); /* service unavailable */
		}
	});
	server.on("/index.html", HTTP_GET, [] (AsyncWebServerRequest * request) {
		if (xSemaphoreTake(mutex, TickType_t(100)) == pdTRUE) {
			request->send(LittleFS, "/web/index.html");
			xSemaphoreGive(mutex);
		} else {
			request->send(503); /* service unavailable */
		}
	});
	server.on("/favicon.ico", HTTP_GET, [] (AsyncWebServerRequest * request) {
		if (xSemaphoreTake(mutex, TickType_t(100)) == pdTRUE) {
			request->send(LittleFS, "/web/favicon.ico");
			xSemaphoreGive(mutex);
		} else {
			request->send(503); /* service unavailable */
		}
	});
	server.on("/config", HTTP_GET, [] (AsyncWebServerRequest * request) {
		/* Send current configuration in JSON format to the client. */
		static const char * fmt = R"({
"mdns": {
	"host": "%s"
}, "ntp": {
	"timeout": %lu,
	"server": "%s"
}, "clock": {
	"passColor": "#%06lX",
	"failColor": "#%06lX",
	"passFrom": "%s",
	"passTo": "%s",
	"type": "%s"
}})";
		AsyncResponseStream * response = request->beginResponseStream("application/json");
		response->addHeader("Cache-Control", "no-cache");
		response->printf(fmt,
			config.mdnsHost,
			config.ntpTimeout,
			config.ntpServer,
			fromRgb565(config.clockPassColor),
			fromRgb565(config.clockFailColor),
			config.clockPassFrom,
			config.clockPassTo,
			config.clockType
		);
		request->send(response);
	});
	server.on("/config", HTTP_POST, [] (AsyncWebServerRequest * request) {
		/* Request completed. Update the current configuration from client configuration (INI format). */
		if (request->_tempObject != NULL && request->contentLength() > 0) {
			enum {
				HAS_MDNS_HOST        = 0x01,
				HAS_NTP_TIMEOUT      = 0x02,
				HAS_NTP_SERVER       = 0x04,
				HAS_CLOCK_PASS_COLOR = 0x08,
				HAS_CLOCK_FAIL_COLOR = 0x10,
				HAS_CLOCK_PASS_FROM  = 0x20,
				HAS_CLOCK_PASS_TO    = 0x40,
				HAS_CLOCK_TYPE       = 0x80
			};
			Config * tmp = static_cast<Config *>(malloc(sizeof(tmp)));
			if ( ! tmp ) {
				request->send(500); /* internal server error */
				return;
			}
			uint32_t found = 0;
			bool (Config::* checker)() const noexcept = NULL;
			const auto valueMapper = [&] (IniParser::Context & ctx, const bool parsed) -> bool {
				checker = NULL;
				if ( ! parsed ) {
					if (ctx.group == "MDNS") {
						if (ctx.key == "HOST") {
							found |= HAS_MDNS_HOST;
							ctx.mapString(tmp->mdnsHost);
							checker = &Config::checkMdnsHost;
						}
					} else if (ctx.group == "NTP") {
						if (ctx.key == "TIMEOUT") {
							found |= HAS_NTP_TIMEOUT;
							ctx.mapNumber(tmp->ntpTimeout, 0, 0xFFFF);
						} else if (ctx.key == "SERVER") {
							found |= HAS_NTP_SERVER;
							ctx.mapString(tmp->ntpServer);
							checker = &Config::checkNtpServer;
						}
					} else if (ctx.group == "CLOCK") {
						if (ctx.key == "PASS_COLOR") {
							found |= HAS_CLOCK_PASS_COLOR;
							ctx.mapNumber(tmp->clockPassColor, 0, 0xFFFF);
						} else if (ctx.key == "FAIL_COLOR") {
							found |= HAS_CLOCK_FAIL_COLOR;
							ctx.mapNumber(tmp->clockFailColor, 0, 0xFFFF);
						} else if (ctx.key == "PASS_FROM") {
							found |= HAS_CLOCK_PASS_FROM;
							ctx.mapString(tmp->clockPassFrom);
							checker = &Config::checkClockPassFrom;
						} else if (ctx.key == "PASS_TO") {
							found |= HAS_CLOCK_PASS_TO;
							ctx.mapString(tmp->clockPassTo);
							checker = &Config::checkClockPassTo;
						} else if (ctx.key == "TYPE") {
							found |= HAS_CLOCK_TYPE;
							ctx.mapString(tmp->clockType);
							checker = &Config::checkClockType;
						}
					}
				} else if (checker != NULL && ((*tmp).*checker)() == false) {
					return false;
				}
				return true; /* ignore unknown keys */
			};
			const size_t res = iniParseString<16>(static_cast<const char *>(request->_tempObject), request->contentLength(), valueMapper);
			int code = 200;
			if (res == 0) {
				if (xSemaphoreTake(mutex, TickType_t(100)) == pdTRUE) {
					bool changed = false;
					if ((found & HAS_MDNS_HOST) && strcmp(config.mdnsHost, tmp->mdnsHost) != 0) {
						memcpy(config.mdnsHost, tmp->mdnsHost, sizeof(config.mdnsHost));
						changed = true;
					}
					if ((found & HAS_NTP_TIMEOUT) && config.ntpTimeout != tmp->ntpTimeout) {
						config.ntpTimeout = tmp->ntpTimeout;
						changed = true;
					}
					if ((found & HAS_NTP_SERVER) && strcmp(config.ntpServer, tmp->ntpServer) != 0) {
						memcpy(config.ntpServer, tmp->ntpServer, sizeof(config.ntpServer));
						state.ntpStarted = false;
						changed = true;
					}
					if ((found & HAS_CLOCK_PASS_COLOR) && config.clockPassColor != tmp->clockPassColor) {
						config.clockPassColor = tmp->clockPassColor;
						state.clockChanged = true;
						changed = true;
					}
					if ((found & HAS_CLOCK_FAIL_COLOR) && config.clockFailColor != tmp->clockFailColor) {
						config.clockFailColor = tmp->clockFailColor;
						state.clockChanged = true;
						changed = true;
					}
					if ((found & HAS_CLOCK_PASS_FROM) && strcmp(config.clockPassFrom, tmp->clockPassFrom) != 0) {
						memcpy(config.clockPassFrom, tmp->clockPassFrom, sizeof(config.clockPassFrom));
						state.clockChanged = true;
						changed = true;
					}
					if ((found & HAS_CLOCK_PASS_TO) && strcmp(config.clockPassTo, tmp->clockPassTo) != 0) {
						memcpy(config.clockPassTo, tmp->clockPassTo, sizeof(config.clockPassTo));
						state.clockChanged = true;
						changed = true;
					}
					if ((found & HAS_CLOCK_TYPE) && config.clockType[0] != tmp->clockType[0]) {
						memcpy(config.clockType, tmp->clockType, sizeof(config.clockType));
						state.clockChanged = true;
						changed = true;
					}
					if ( changed ) {
						state.configChanged = true;
					}
					xSemaphoreGive(mutex);
				} else {
					code = 503; /* service unavailable */
				}
			} else {
				code = 400; /* bad request */
			}
			request->send(code);
			free(tmp);
		} else {
			request->send(204); /* no content */
		}
	}, NULL /* handleUpload() */,
	[] (AsyncWebServerRequest * request, uint8_t * bodyData, size_t bodyLen, size_t index, size_t total) {
		/* Ongoing reception of the body data. */
		if (total > 0 && request->_tempObject == NULL && total < 2048) {
			request->_tempObject = malloc(total); /* automatically freed by `AsyncWebServerRequest` */
		}
		if (request->_tempObject != NULL) {
			memcpy(static_cast<uint8_t *>(request->_tempObject) + index, bodyData, bodyLen);
		}
	});
	server.on("/reboot", HTTP_POST, [] (AsyncWebServerRequest * request) {
		/* Reboot the ESP32. */
		request->send(200);
		delay(200);
		ESP.restart();
	});
	server.onNotFound([] (AsyncWebServerRequest * request) {
		if (request->method() == HTTP_OPTIONS) {
			request->send(200);
		} else {
			request->send(404, "text/plain", "Page not found");
		}
	});
	server.begin();
	/* buttons */
	pinMode(BUTTON_UP, INPUT);
	pinMode(BUTTON_DOWN, INPUT);
	bUpLast = millis();
	bDownLast = millis();
	attachInterrupt(BUTTON_UP, buttonUpIsr, FALLING);
	attachInterrupt(BUTTON_DOWN, buttonDownIsr, FALLING);
	log_e("free heap: %lu bytes", static_cast<unsigned long>(ESP.getFreeHeap()));
}


/**
 * Updates the system state and displays the current time.
 */
void loop() {
#define LOOP_DELAY_MS 1000
	const bool lockTaken = (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE);
	State newState = state;
	const bool clockChanged = newState.clockChanged;
	newState.clockChanged = false;
	newState.configChanged = false;
	newState.wifiOnline = (WiFi.status() == WL_CONNECTED);
	if (state.wifiOnline && !newState.wifiOnline) {
		/* went offline -> reset states */
		newState.setOffline();
	}
	if ( newState.wifiOnline ) {
		/* Over-the-Air updater */
		if ( ! newState.otaStarted ) {
			/* setup updater */
			ArduinoOTA.begin();
			MDNS.addService("http", "tcp", 80);
			newState.otaStarted = true;
		}
		ArduinoOTA.handle();
		/* NTP client */
		if ( ! newState.ntpStarted ) {
			/* setup client */
			NTP.setNTPTimeout(uint16_t(config.ntpTimeout));
			NTP.setInterval(63);
			NTP.setTimeZone(TZ_Europe_Berlin);
			NTP.begin(config.ntpServer);
			newState.ntpStarted = true;
		}
	}
	if (newState.wifiOnline && newState.ntpStarted) {
		/* all up and running -> update time */
		newState.updateNtp();
	}
	if (memcmp(&state, &newState, sizeof(newState)) != 0) {
		/* state changed */
		if ( state.configChanged ) {
			/* Store updated configuration.
			 * This is not done within the web server to as it takes long and
			 * degenerates the flash if done too often.
			 */
			if ( ! config.store() ) {
				log_e("Failed to store new configuration on flash.");
			}
		}
		state = newState;
		if ( lockTaken ) {
			xSemaphoreGive(mutex);
		}
		/* update display */
		if (config.clockType[0] == 'd') {
			/* display digital clock */
			if ( clockChanged ) {
				tft.fillScreen(TFT_BLACK);
			}
			tft.setTextColor(
				uint16_t(
					newState.withinTimeSpan(config.clockPassFrom, config.clockPassTo)
						? config.clockPassColor : config.clockFailColor
				),
				TFT_BLACK
			);
			tft.drawString(newState.time, 160, 120);
		} else {
			/* display analog clock */
			const float hour = float((10 * (newState.time[0] - '0')) + (newState.time[1] - '0'));
			const float min = float((10 * (newState.time[3] - '0')) + (newState.time[4] - '0'));
			/* adjust angle of clock hands */
			svgRotateShape("hour", (hour + (min / 60.0f)) * 30.0f);
			svgRotateShape("min", min * 6.0f);
			/* set colors */
			const uint16_t rgb565 = uint16_t(newState.withinTimeSpan(config.clockPassFrom, config.clockPassTo)
				? config.clockPassColor : config.clockFailColor);
			svgSetFill("circle", svgFromRgb565(rgb565));
			/* raster SVG */
			nsvgRasterize(svgRast, svgImg, 0, 0, 1, imgBuf, 320, 240, 320 * 4);
			/* convert RGBA32 to RGB565 in-place */
			size_t i = 0;
			uint16_t * ptr = reinterpret_cast<uint16_t *>(imgBuf);
			for (size_t y = 0; y < 240; y++) {
				for (size_t x = 0; x < 320; x++) {
					const uint8_t r = imgBuf[i++];
					const uint8_t g = imgBuf[i++];
					const uint8_t b = imgBuf[i++];
					i++;
					*ptr++ = uint16_t((uint16_t(r & 0xF8) << 8) | (uint16_t(g & 0xFC) << 3) | (b >> 3));
				}
			}
			/* flush to screen */
			tft.pushImage(0, 0, 320, 240, reinterpret_cast<uint16_t *>(imgBuf));
			/* restore clock hands angle */
			svgSetPaths("hour", svgPathsHour);
			svgSetPaths("min", svgPathsMin);
		}
	} else {
		if ( lockTaken ) {
			xSemaphoreGive(mutex);
		}
		/* keep power consumption low by doing less updates */
		delay(LOOP_DELAY_MS);
	}
}
