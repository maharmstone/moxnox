#pragma once

// musl wants a spinlock for random(), against the spec
#define LOCK(x)
#define UNLOCK(x)
