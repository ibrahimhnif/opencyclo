#ifndef OPENCYCLO_HARDWARE_WEB_SERIAL_H
#define OPENCYCLO_HARDWARE_WEB_SERIAL_H

// Browser companion over USB CDC (Web Serial API). Line protocol, 115200:
//   host -> device: "OC+COMMAND\n" or "OC+COMMAND:<payload>\n"
//   device -> host:  "OC=<json>\n" (one line per command; log lines never
//                   start with "OC=", so the browser filters on the prefix).
//
// Commands: HELLO, GET_LAYOUT, SET_LAYOUT:<layout json>, GET_THEME,
// SET_THEME:<theme json>, RESET_ALL. SET_* stage and validate before
// touching live state, mirroring the BLE import contract.
void startWebSerialTask();

#endif  // OPENCYCLO_HARDWARE_WEB_SERIAL_H
