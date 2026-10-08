#include <doctest/doctest.h>
#include "b21ui/Abi.h"
#include "core/Version.h"

TEST_CASE("ABI minimum sizes cover every v1 field") {
    CHECK(B21UI_RENDEZVOUS_MIN_SIZE == sizeof(B21UI_Rendezvous));
    CHECK(B21UI_HOSTAPI_MIN_SIZE == offsetof(B21UI_HostApi, setPausesGame));
    CHECK(B21UI_HOSTAPI_MIN_SIZE < sizeof(B21UI_HostApi));
    CHECK(offsetof(B21UI_HostApi, queueGameTask) == offsetof(B21UI_HostApi, setPausesGame) + sizeof(void*));
    CHECK(B21UI_CLIENTDESC_MIN_SIZE == sizeof(B21UI_ClientDesc));
    CHECK(b21ui::kFrameworkVersion >= 1u);
}
