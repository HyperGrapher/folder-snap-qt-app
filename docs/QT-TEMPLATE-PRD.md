Build a Windows 10/11 desktop GUI proof of concept using **C++ and Qt Quick/QML**.

This project is about validating the interface, animation quality, and rendering performance. It does not need real application functionality. If successful, I may use it as the foundation for a future desktop app, so keep the implementation clean and reusable.

**Visual direction**

The inspiration is the macOS app **CleanMyMac**, specifically its vertical sidebar navigation, smooth transitions between sections, polished interactive elements, and soft animated backgrounds that look like flowing shader blobs.

Use those qualities as design inspiration. Create an original interface rather than copying its branding or assuming this is a cleaning utility. Use mock content to demonstrate the visual system.

The result should feel cohesive, refined, and responsive. Avoid a generic dashboard with default controls and animation added as an afterthought.

**1. Technology and structure**

* Use C++ for application logic, QML/Qt Quick for the interface, and CMake for building.
* Use Qt Quick Controls with consistent custom styling.
* Implement the animated background using Qt Shader Tools and `ShaderEffect`.
* Do not use Electron, WebView, Chromium, or an embedded web frontend.
* Separate the window shell, navigation, pages, reusable controls, and background effects.
* Centralize colors, spacing, corner radii, typography, animation durations, and easing choices.

**2. Custom application window**

Create a frameless window with a custom title area, rounded outer corners, and integrated minimize, maximize/restore, and close buttons.

Preserve expected Windows behavior: dragging, resizing, double-clicking the title area to maximize/restore, and correct maximized sizing. Interactive controls must not accidentally drag the window.

Handle Windows 10 and Windows 11 differences explicitly. A rounded rectangle painted inside a rectangular opaque window does not satisfy the rounded-window requirement. Use an appropriate supported implementation and explain any platform limitations.

Support display scaling and keep window controls clear and usable.

**3. Vertical navigation and page transitions**

Create a persistent left sidebar with approximately four demonstration tabs, each with an icon and label.

* Give the active tab a distinctive animated selection treatment.
* Animate the content area when switching tabs.
* Start with a restrained combination of opacity and a small directional movement.
* Keep the sidebar and window shell stable during page transitions.
* Ensure outgoing and incoming content transition without blank flashes, abrupt layout jumps, or unnecessary page resets.
* Handle rapid tab switching gracefully: cancel or retarget transitions instead of queuing a long sequence.
* Ensure invisible or outgoing pages cannot receive unintended input.

Use different mock layouts to demonstrate transitions between a summary page, a card collection, a progress/status page, and settings.

**4. Animated shader background**

Create a soft, slowly moving background with several organic color blobs that blend smoothly into one another.

The effect should resemble an ambient flowing gradient: subtle movement, gentle shape changes, and smooth color transitions. Avoid hard edges, noisy textures, particles, flashing, and distracting motion.

Keep foreground text readable. Coordinate the background palette with the selected section, using smooth color changes when switching tabs.

Use GPU rendering and avoid expensive full-window blur stacks where a simpler shader can achieve the appearance.

**5. Buttons and animated widgets**

Build reusable buttons with polished hover, pressed, keyboard-focus, and disabled states. Use restrained changes in color, scale, or elevation.

Include a few demonstration widgets, such as:

* A progress ring with an animated value.
* A status card that changes state.
* A toggle with a smooth transition.
* A card with a subtle hover response.

Use local mock state and explicit demo actions to trigger these animations. No real scanning, cleaning, networking, or system modification is required.

**6. Performance and motion**

Keep CPU, RAM, and GPU use economical.

Stop decorative animation when the window is minimized or hidden. Provide a reduced-motion mode that replaces continuous background movement and large transitions with static or minimal alternatives.

Avoid unnecessary timers, repeated layout work, and unbounded resource caching. Keep ordinary transitions short and responsive; ambient background movement should be much slower.

**7. Completion criteria**

Deliver a runnable application, not just static mockups or a plan.

Verify tab switching, rapid repeated navigation, widget interactions, resizing, maximizing/restoring, and display scaling. Check for clipping, flicker, input problems, and animation stutter.

Provide build/run instructions, explain where visual settings are configured, and report any platform behavior you could not test. Measure resource usage where possible and distinguish actual measurements from estimates.
