# Esp32 range finder receiver

Esp32 range finder receiver is a device designed to test acceptable communication range between 2 esp32/esp8266 devices.

It is using EspNow protocol built over 2.4GHz and with less overhead than wifi (obviously).

<br><br>
<div align="center">

![portable device](./img/esp32-tv.jpg?raw=true "Hardware")
![Test result](./img/range-finder-test.jpg?raw=true "Test result")
</div>

## Features
* Portable device, battery powered
* Built in screen (TFT 2.4")
* EspNow protocol over 2.4GHz
* Set up basic communication (32 bits message, 16 for header/command and 16 for payload)
* Set up test behaviour (command 0 = test start, command 1 = test packet, command 2 = test end)
* Display reception rate
* Includes real time (dual core) programming using RTOS. That's why you will see semaphore and tasks stuff.

With proper antenna orientation and no interference environment (open field, decent distance from houses) 100-200m should be easily achievable. With directional antenna that range could bump up to 2 to 3 times more (keep in mind this is very low overhead and very small packets).

One problem that may affect performance in this situation is the induced waves in the electronics. It is very recommended to get a better antenna WITH cable to make some distance between antenna and the mcu.

## Want to test?

1. :open_file_folder: Clone or download.
2. :memo: Add project to PlatformIO (it's a VSCode or Atom extension).
3. :memo: Configure your board and platform on `platformio.ini` (all configurations, if neccesary create new project from ui and copy from it).
4. :memo: Configure constants on top of `main.cpp`
5. :ballot_box_with_check: Select the `env:debug` configuration.
6. :rocket: Hit `build`, `upload` and `monitor`.
7. :sparkles: Watch the serial output or checkout the sdcard after a while.

If you have any question about something specific, open an issue (but i won't be a teacher).