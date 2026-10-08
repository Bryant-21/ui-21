#pragma once
// Compatibility name from before the style split: kit:: = the shared core (b21ui/Common.h) plus the
// fo76 look (b21ui/fo76/Style.h). New menus include one style header instead.
#include "b21ui/Common.h"
#include "b21ui/fo76/Style.h"

namespace b21ui::kit {
    using namespace ::b21ui::common;
    using namespace ::b21ui::fo76;
}
