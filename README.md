# Sirokko
Automation of an Sirokko OETF10N with an ESP32 Microprocessor

Automation for the Starting and Cool Down Process of an old Sirokko Diesel Heating Device.
The automation uses an Heltec ESP32 Wifi Kit, with included OLED Display.

The microprocessor measures the supply and exhaust air temperatures. 
It uses three relays to switch a solenoid valve for the oil supply, the glow plug and the fan.
The processor can regulate the room temperature within rough limits using a simple two-point control.

A simple flame monitor prevents the heater from filling up with oil if it does not start.
