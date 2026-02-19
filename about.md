# Chaos Mod GD

---

Made by [Polik](https://gdbrowser.com/u/polikyt)

---
## About

Chaos mod GD adds random events every time the timer hits 0. The events vary from jumpscares to speed increases.
You can change the countdown time in the settings.

## Events

Chaos Mod GD v1.0.0 currently has these events:
 * speed x0.5
 * speed x1.5
 * speed x2
 * gravity low
 * gravity high
 * reverse controls
 * size mini
 * jump delay
 * soggy jumpscare
 * invert colors
 * shake screen
 * high pitch
 * low pitch
 * drunk mode
 * snow screen
 * flip vertical
 * flip horizontal

Expect to see more events in future updates!

---

## Developers

### Adding a New Event

To add a new chaos event to the mod, follow these steps:

1. **Create the event file**: Create a new `.cpp` file in the `src/events/` directory (e.g., `MyEvent.cpp`).

2. **Implement the event logic**: 
   - Include the necessary headers: `#include "Event.hpp"` and `#include <Geode/Geode.hpp>`
   - Define a namespace: `namespace chaosmod { ... }`
   - Create a function that applies your event effect, taking a `PlayLayer*` parameter
   - If your event has a duration, define a constant like `static constexpr float kEventDuration = 10.f;`
   - Handle cleanup properly (restore original state when the event ends)

3. **Create the register function**: At the end of your file, add a register function:
   ```cpp
   void registerMyEvent(EventRegistry& reg) {
       reg.add(EventDef(
           "my-event-id",        // Unique ID (use kebab-case)
           "My Event Name",      // Display name
           kEventDuration,       // Duration in seconds (0.f for instant events)
           [](PlayLayer* pl) {   // Lambda function that runs the event
               // Your event logic here
               applyMyEffect(pl, kEventDuration);
           }
       ));
   }
   ```

4. **Register the event globally**:
   - Open `src/events/RegisterAll.cpp`
   - Add a forward declaration: `void registerMyEvent(EventRegistry&);`
   - Add the call in `registerAllEvents()`: `registerMyEvent(reg);`

5. **Add to settings** (optional, for debug forcing):
   - Open `mod.json`
   - Add your event ID to the `"one-of"` array in the `"forced-event-id"` setting

**Example structure for a simple event:**
```cpp
#include "Event.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 15.f;

static void applyMyEffect(PlayLayer* pl, float duration) {
    // Implement your effect here
    // Remember to schedule cleanup after 'duration' seconds
}

void registerMyEvent(EventRegistry& reg) {
    reg.add(EventDef(
        "my-event",
        "My Event",
        kEventDuration,
        [](PlayLayer* pl) {
            applyMyEffect(pl, kEventDuration);
        }
    ));
}

}
```