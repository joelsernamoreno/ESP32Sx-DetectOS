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

enum HostOS {
  OS_UNKNOWN,
  OS_WINDOWS,
  OS_LINUX,
  OS_MACOS,
  OS_IOS
};

HostOS detected_os = OS_UNKNOWN;

void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == ARDUINO_USB_HID_KEYBOARD_EVENTS) {
    arduino_usb_hid_keyboard_event_data_t* data =
      (arduino_usb_hid_keyboard_event_data_t*)event_data;

    if (event_id == ARDUINO_USB_HID_KEYBOARD_LED_EVENT) {
      led_response_received = true;
      led_event_count++;
      caps_status = data->capslock;
      num_status = data->numlock;
      scroll_status = data->scrolllock;

      SerialUSB.printf("LED event %d: Caps:%u Num:%u Scroll:%u\n", led_event_count, caps_status, num_status, scroll_status);
    }
  }
}

void toggleKey(uint8_t key) {
  Keyboard.press(key);
  delay(300);
  Keyboard.release(key);
  delay(500);
}

void resetKeyboardLEDs() {
  SerialUSB.println("Resetting keyboard LEDs...");
  led_response_received = false;
  toggleKey(KEY_CAPS_LOCK);
  delay(500);
  if (caps_status) toggleKey(KEY_CAPS_LOCK);

  led_response_received = false;
  toggleKey(KEY_NUM_LOCK);
  delay(500);
  if (num_status) toggleKey(KEY_NUM_LOCK);

  led_response_received = false;
  toggleKey(KEY_SCROLL_LOCK);
  delay(500);
  if (scroll_status) toggleKey(KEY_SCROLL_LOCK);

  SerialUSB.printf("LEDs after reset: Caps:%u Num:%u Scroll:%u\n",
                  caps_status, num_status, scroll_status);
}

void detectHostOS() {
  SerialUSB.println("=== Starting OS detection ===");

  led_event_count = 0;
  led_response_received = false;
  caps_status = 0;
  num_status = 0;
  scroll_status = 0;

  toggleKey(KEY_CAPS_LOCK);
  delay(800);

  if (!led_response_received) {
    if (caps_status) {
      detected_os = OS_IOS;
      SerialUSB.println("iOS detected (CapsLock active without event)");
      return;
    }
    
    toggleKey(KEY_CAPS_LOCK);
    delay(800);
    
    if (!led_response_received) {
      if (caps_status) {
        detected_os = OS_IOS;
        SerialUSB.println("iOS detected (CapsLock active after retry)");
      } else {
        detected_os = OS_MACOS;
        SerialUSB.println("macOS detected (no LED response)");
      }
      return;
    }
  }

  bool windows_detected = false;
  if (led_event_count >= 1) {
    toggleKey(KEY_NUM_LOCK);
    delay(800);
    toggleKey(KEY_SCROLL_LOCK);
    delay(800);
    
    if (led_event_count >= 3) {
      windows_detected = true;
    }
  }

  if (windows_detected) {
    detected_os = OS_WINDOWS;
    SerialUSB.println("Windows detected (multiple LED responses)");
    return;
  }

  led_response_received = false;
  toggleKey(KEY_NUM_LOCK);
  delay(800);
  bool num_response = led_response_received;

  led_response_received = false;
  toggleKey(KEY_SCROLL_LOCK);
  delay(800);
  bool scroll_response = led_response_received;

  if (num_response || scroll_response) {
    detected_os = OS_LINUX;
    SerialUSB.println("Linux detected (partial LED response)");
  } else if (caps_status) {
    detected_os = OS_IOS;
    SerialUSB.println("iOS detected (only CapsLock active)");
  } else {
    detected_os = OS_UNKNOWN;
    SerialUSB.println("OS detection failed");
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
      SerialUSB.println("Detected: Windows");
      Keyboard.println("Windows detected");
      break;
    case OS_LINUX:
      SerialUSB.println("Detected: Linux");
      Keyboard.println("Linux detected");
      break;
    case OS_MACOS:
      SerialUSB.println("Detected: macOS");
      Keyboard.println("macOS detected");
      break;
    case OS_IOS:
      SerialUSB.println("Detected: iOS");
      Keyboard.println("iOS detected");
      break;
    default:
      SerialUSB.println("OS Unknown");
      Keyboard.println("OS Unknown");
      break;
  }
  delay(5000);
}