#pragma once
#include <algorithm>
// Keep notification + ATT/L2CAP headers within one 255-byte host buffer.
// A negotiated ATT MTU of 512 does not imply enough buffers for 488-byte bursts.
inline unsigned rideExportPayloadLimit(unsigned mtu) {
  return mtu>11?std::min(180u,mtu-11):0;
}
