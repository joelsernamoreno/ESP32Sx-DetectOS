#include <USB.h>
#include <USBHIDKeyboard.h>

USBHIDKeyboard Keyboard;
USBCDC USBSerial;

String os;
volatile bool led_response_received = false;
volatile int led_event_count = 0;
volatile unsigned long led_event_time = 0;
volatile bool caps_status = false;
volatile bool num_status = false;
volatile bool scroll_status = false;
volatile bool numlock_checked = false;
volatile bool os_detection_complete = false;
unsigned long caps_sent_time = 0;
unsigned long num_sent_time = 0;
unsigned long scroll_sent_time = 0;
unsigned long caps_delay = 0;
unsigned long num_delay = 0;
unsigned long scroll_delay = 0;

enum HostOS {
  OS_UNKNOWN,
  OS_WINDOWS,
  OS_LINUX,
  OS_MACOS,
  OS_IOS,
  OS_ANDROID,
  OS_CHROMEOS
};

HostOS detected_os = OS_UNKNOWN;

void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == ARDUINO_USB_HID_KEYBOARD_EVENTS) {
    arduino_usb_hid_keyboard_event_data_t *data = (arduino_usb_hid_keyboard_event_data_t *)event_data;

    if (event_id == ARDUINO_USB_HID_KEYBOARD_LED_EVENT) {
      led_response_received = true;
      led_event_count++;
      led_event_time = millis();

      caps_status = data->capslock != 0;
      num_status = data->numlock != 0;
      scroll_status = data->scrolllock != 0;
      numlock_checked = true;

      if (caps_sent_time > 0 && caps_delay == 0)
        caps_delay = led_event_time - caps_sent_time;
      if (num_sent_time > 0 && num_delay == 0)
        num_delay = led_event_time - num_sent_time;
      if (scroll_sent_time > 0 && scroll_delay == 0)
        scroll_delay = led_event_time - scroll_sent_time;
    }
  }
}

void toggleKey(uint8_t key, unsigned long *send_time) {
  *send_time = millis();
  led_response_received = false;

  Keyboard.press(key);
  delay(300);
  Keyboard.release(key);
  delay(800);
}

void resetKeyboardLEDs() {
  unsigned long temp_time;
  delay(500);
  if (caps_status) {
    toggleKey(KEY_CAPS_LOCK, &temp_time);
    delay(800);
  }
  if (num_status) {
    toggleKey(KEY_NUM_LOCK, &temp_time);
    delay(800);
  }
  if (scroll_status) {
    toggleKey(KEY_SCROLL_LOCK, &temp_time);
    delay(800);
  }
}

void detectHostOS() {
  led_event_count = 0;
  caps_status = num_status = scroll_status = 0;
  caps_delay = num_delay = scroll_delay = 0;
  caps_sent_time = num_sent_time = scroll_sent_time = 0;
  led_response_received = false;
  uint8_t initial_caps = caps_status;

  Keyboard.println("=== USB HID OS Detector ===");
  Keyboard.println("[DEBUG] Starting OS detection...");

  // CAPS LOCK
  Keyboard.println("[DEBUG] Testing Caps Lock...");
  toggleKey(KEY_CAPS_LOCK, &caps_sent_time);
  delay(1500);
  Keyboard.print("[DEBUG] CAPS LOCK - RESPONSE: ");
  Keyboard.println(led_response_received ? "YES" : "NO");
  Keyboard.print("[DEBUG] CAPS STATUS: ");
  Keyboard.println(caps_status);
  Keyboard.print("[DEBUG] INITIAL CAPS: ");
  Keyboard.println(initial_caps);
  Keyboard.print("[DEBUG] CAPS DELAY: ");
  Keyboard.println(caps_delay);

  // iOS early detection
  if (!led_response_received && caps_status != initial_caps) {
    Keyboard.println("[DEBUG] FINAL DECISION: iOS (EARLY DETECTION)");
    detected_os = OS_IOS;
    return;
  }

  // NUM LOCK
  Keyboard.println("[DEBUG] Testing Num Lock...");
  toggleKey(KEY_NUM_LOCK, &num_sent_time);
  delay(1200);
  Keyboard.print("[DEBUG] NUM LOCK - RESPONSE: ");
  Keyboard.println(num_delay > 0 ? "YES" : "NO");
  Keyboard.print("[DEBUG] NUM STATUS: ");
  Keyboard.println(num_status);
  Keyboard.print("[DEBUG] NUM DELAY: ");
  Keyboard.println(num_delay);

  // SCROLL LOCK
  Keyboard.println("[DEBUG] Testing Scroll Lock...");
  toggleKey(KEY_SCROLL_LOCK, &scroll_sent_time);
  delay(1200);
  Keyboard.print("[DEBUG] SCROLL LOCK - RESPONSE: ");
  Keyboard.println(scroll_delay > 0 ? "YES" : "NO");
  Keyboard.print("[DEBUG] SCROLL STATUS: ");
  Keyboard.println(scroll_status);
  Keyboard.print("[DEBUG] SCROLL DELAY: ");
  Keyboard.println(scroll_delay);

  Keyboard.print("[DEBUG] LED EVENTS DETECTED: ");
  Keyboard.println(led_event_count);

  if (led_event_count == 0) {
    detected_os = (caps_status != initial_caps) ? OS_IOS : OS_MACOS;
    Keyboard.print("[DEBUG] FINAL DECISION: ");
    Keyboard.println(detected_os == OS_IOS ? "iOS" : "macOS");
  }
  else if (led_event_count >= 3 && caps_delay < 100 && num_delay < 100 && scroll_delay < 100) {
    detected_os = OS_WINDOWS;
    Keyboard.println("[DEBUG] FINAL DECISION: Windows");
  }
  else {
    bool has_numlock_response = (num_delay > 0);
    bool has_scrolllock_response = (scroll_delay > 0);

    // ChromeOS detection
    if (led_event_count == 1 &&
        caps_status != initial_caps &&
        caps_delay > 0 && caps_delay < 20 &&
        !has_numlock_response && !has_scrolllock_response) {
      detected_os = OS_CHROMEOS;
      Keyboard.println("[DEBUG] FINAL DECISION: ChromeOS");
    }
    // Linux detection
    else if (num_status && !scroll_status &&
             has_numlock_response && !has_scrolllock_response) {
      detected_os = OS_LINUX;
      Keyboard.println("[DEBUG] FINAL DECISION: Linux");
    }
    // Android detection
    else if ((caps_delay > 200 || num_delay > 200 || scroll_delay > 200) &&
             (has_numlock_response || has_scrolllock_response)) {
      detected_os = OS_ANDROID;
      Keyboard.println("[DEBUG] FINAL DECISION: Android");
    }
    // Fallback to iOS
    else if (caps_status != initial_caps) {
      detected_os = OS_IOS;
      Keyboard.println("[DEBUG] FINAL DECISION: iOS (FALLBACK)");
    }
    // Unknown
    else {
      detected_os = OS_UNKNOWN;
      Keyboard.println("[DEBUG] FINAL DECISION: Unknown OS");
    }
  }
}

void printDetectedOS() {
  switch (detected_os) {
    case OS_WINDOWS:
      os = "Windows";
      Keyboard.println("Windows");
      break;
    case OS_LINUX:
      os = "Linux";
      Keyboard.println("Linux");
      break;
    case OS_MACOS:
      os = "macOS";
      Keyboard.println("macOS");
      break;
    case OS_IOS:
      os = "iOS";
      Keyboard.println("iOS");
      break;
    case OS_ANDROID:
      os = "Android";
      Keyboard.println("Android");
      break;
    case OS_CHROMEOS:
      os = "ChromeOS";
      Keyboard.println("chromeOS");
      break;
    default:
      os = "OS Unknown";
      Keyboard.println("OS Unknown");
      break;
  }
  os_detection_complete = true;
}

void onDetectOSRequested() {
  resetKeyboardLEDs();
  detectHostOS();
  resetKeyboardLEDs();
  printDetectedOS();
}

void setup() {
  USB.onEvent(usbEventCallback);
  Keyboard.onEvent(usbEventCallback);
  USBSerial.onEvent(usbEventCallback);
  USB.begin();
  Keyboard.begin();
  USBSerial.begin();
  delay(2000);
  USBSerial.println("=== USB HID OS Detector ===");
  onDetectOSRequested();
}

void loop() {
}