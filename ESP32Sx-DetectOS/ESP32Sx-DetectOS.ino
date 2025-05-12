#include <USB.h>
#include <USBHIDKeyboard.h>

#define KEY_NUM_LOCK     0xDB
#define KEY_CAPS_LOCK    0xC1
#define KEY_SCROLL_LOCK  0xCF

USBHIDKeyboard Keyboard;
USBCDC SerialUSB;

volatile bool led_response_received = false;
volatile uint8_t caps_status = 0;
volatile uint8_t num_status = 0;
volatile uint8_t scroll_status = 0;
volatile int led_event_count = 0;
volatile unsigned long led_event_time = 0;
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
  OS_ANDROID
};

HostOS detected_os = OS_UNKNOWN;

void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == ARDUINO_USB_HID_KEYBOARD_EVENTS) {
    arduino_usb_hid_keyboard_event_data_t* data = (arduino_usb_hid_keyboard_event_data_t*)event_data;

    if (event_id == ARDUINO_USB_HID_KEYBOARD_LED_EVENT) {
      led_response_received = true;
      led_event_count++;
      led_event_time = millis();

      if (caps_sent_time > 0 && caps_delay == 0)
        caps_delay = led_event_time - caps_sent_time;
      if (num_sent_time > 0 && num_delay == 0)
        num_delay = led_event_time - num_sent_time;
      if (scroll_sent_time > 0 && scroll_delay == 0)
        scroll_delay = led_event_time - scroll_sent_time;

      caps_status = data->capslock;
      num_status = data->numlock;
      scroll_status = data->scrolllock;

      SerialUSB.printf("LED event %d: Caps:%u Num:%u Scroll:%u (Delay: %lums)\n",
                     led_event_count, caps_status, num_status, scroll_status, led_event_time);
    }
  }
}

void toggleKey(uint8_t key, unsigned long* send_time) {
  *send_time = millis();
  led_response_received = false;

  Keyboard.press(key);
  delay(300);
  Keyboard.release(key);
  delay(800);
}

void resetKeyboardLEDs() {
  SerialUSB.println("Resetting keyboard LEDs...");
  
  unsigned long temp_time;
  if (caps_status) toggleKey(KEY_CAPS_LOCK, &temp_time);
  if (num_status) toggleKey(KEY_NUM_LOCK, &temp_time);
  if (scroll_status) toggleKey(KEY_SCROLL_LOCK, &temp_time);
  
  SerialUSB.printf("LEDs after reset: Caps:%u Num:%u Scroll:%u\n",
                  caps_status, num_status, scroll_status);
}

void detectHostOS() {
  SerialUSB.println("=== Starting OS detection ===");
  led_event_count = 0;
  caps_status = num_status = scroll_status = 0;
  caps_delay = num_delay = scroll_delay = 0;
  caps_sent_time = num_sent_time = scroll_sent_time = 0;
  led_response_received = false;

  // Test Caps Lock (iOS detection)
  uint8_t initial_caps = caps_status;
  toggleKey(KEY_CAPS_LOCK, &caps_sent_time);
  delay(1200); // Increased delay for iOS
  
  // Special iOS check - Caps Lock changes but no LED event
  if (!led_response_received && caps_status != initial_caps) {
    detected_os = OS_IOS;
    SerialUSB.println("iOS detected (CapsLock changed without LED event)");
    return;
  }

  // Test Num Lock
  toggleKey(KEY_NUM_LOCK, &num_sent_time);
  delay(800);

  // Test Scroll Lock
  toggleKey(KEY_SCROLL_LOCK, &scroll_sent_time);
  delay(800);

  // Enhanced decision logic
  if (led_event_count == 0) {
    if (caps_status != initial_caps) {
      detected_os = OS_IOS;
      SerialUSB.println("iOS detected (no LED events but CapsLock changed)");
    } else {
      detected_os = OS_MACOS;
      SerialUSB.println("macOS detected (no response)");
    }
  }
  else if (caps_delay > 200 || num_delay > 200 || scroll_delay > 200) {
    detected_os = OS_ANDROID;
    SerialUSB.println("Android detected (delayed responses)");
  }
  else if (led_event_count >= 3 && caps_delay < 100 && num_delay < 100 && scroll_delay < 100) {
    detected_os = OS_WINDOWS;
    SerialUSB.println("Windows detected (fast responses)");
  }
  else {
    if (num_status || scroll_status) {
      detected_os = OS_LINUX;
      SerialUSB.println("Linux detected (partial LED responses)");
    } else if (caps_status != initial_caps) {
      detected_os = OS_IOS;
      SerialUSB.println("iOS detected (only CapsLock changed)");
    } else {
      detected_os = OS_UNKNOWN;
      SerialUSB.println("OS detection failed");
    }
  }
}

void setup() {
  USB.onEvent(usbEventCallback);
  Keyboard.onEvent(usbEventCallback);
  SerialUSB.onEvent(usbEventCallback);
  USB.begin();
  Keyboard.begin();
  SerialUSB.begin();
  delay(2000);
  SerialUSB.println("=== USB HID OS Detector ===");
  resetKeyboardLEDs();
  detectHostOS();
  resetKeyboardLEDs();
}

void loop() {
  switch (detected_os) {
    case OS_WINDOWS:
      SerialUSB.println("Windows detected");
      Keyboard.println("Windows detected");
      break;
    case OS_LINUX:
      SerialUSB.println("Linux detected");
      Keyboard.println("Linux detected");
      break;
    case OS_MACOS:
      SerialUSB.println("macOS detected");
      Keyboard.println("macOS detected");
      break;
    case OS_IOS:
      SerialUSB.println("iOS detected");
      Keyboard.println("iOS detected");
      break;
    case OS_ANDROID:
      SerialUSB.println("Android detected");
      Keyboard.println("Android detected");
      break;
    default:
      SerialUSB.println("OS Unknown");
      Keyboard.println("OS Unknown");
      break;
  }
  delay(5000);
}