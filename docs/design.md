# Why it looks like this

The README says what ClaudeDial does. This says why the visual decisions came
out the way they did, so that nobody — including a later version of the author —
quietly undoes one of them for looking arbitrary.

Every claim here was settled by rendering the thing into a real Plasma panel and
looking at it, not by enlarging a mock-up. At 16–24 px a magnified mock-up is
actively misleading: the first version of the mark read as a speedometer when
blown up and as a grey loading spinner in the panel.

## One dial, everywhere

Six renditions — the application icon, the popup header, the settings window's
icon, both tray styles and the notification icon — come from the canonical
numbers in [`src/core/GaugeGeometry.h`](../src/core/GaugeGeometry.h): a
240-degree arc from 210° to −30°, gap at the bottom, one outer radius, one
centre.

They are pure numbers with no Qt drawing types, so both the painter and the unit
test that checks the shipped SVG can read them. That test exists because the SVG
cannot share code, only numbers, and it had already drifted: its centre sat 7.8%
lower and its outer edge 5% smaller than every other rendition. That is
measurable, and it was exactly why the identity mark and the popup's mark had
stopped looking like the same thing.

**Stroke weight is the one free variable**, and the geometry is arranged so it
costs nothing: the inset is half the stroke plus a fixed margin, so the arc's
outer silhouette is invariant to weight. A lighter rendition is the same mark,
not a similar one.

The percentage style deviates further, down to 0.32 of full weight, for a
measured reason: a two-digit number is a rectangle inscribed in the arc's
circle, so the interior radius has to cover half its diagonal. At full weight it
does not, and `88` and `99` collide with the arc walls. The radius cannot help —
the arc already fills the icon box, leaving about 5% to gain — so the room had to
come from the stroke.

## The tray icon

**A full ring was tried first, and read as a notification badge** or a battery
indicator. The 240-degree arc with the gap at the bottom is what makes it an
instrument.

**At 100% the Percentage style shows `!`.** Three digits were tried at a reduced
size; in a real panel they came out visibly weaker than `99`, the one reading
that most needs to carry. A full red arc around an exclamation mark says it more
firmly, and the tooltip still gives the figure.

**Digits are 0.58 of the icon, DemiBold.** Bold at 0.68 was tried and made the
icon the loudest thing in the tray — it started reading as a notification badge
again. Medium goes thin enough to fade at 16 px, particularly in the warning and
critical colours.

**The needle is held clear of the arc by a visible gap.** At a 1.5 px gap it and
the usage fill merge into a single lump at low percentages.

## The colour ramp

Steps at the warning threshold, the critical threshold, 95% and 100% — the same
points the notifications fire on, so what the user sees and what they are told
agree.

Jumping from amber straight to red at the critical threshold was tried. It made
every reading from 90 to 100 look identical, collapsing four distinct states
into one colour, and the 89→90 transition read as an abrupt jump. The orange step
fixes both.

The tray stays monochrome below the warning threshold because a panel icon is on
screen permanently and must not be a standing splash of colour. The popup's
first coloured step differs from the tray's for the same reason inverted: it is
only visible while being read.

## Three colour roles

| Role | Where |
| --- | --- |
| Terracotta | ClaudeDial's identity: the application icon and the popup's header mark |
| The user's Plasma accent | interactive controls |
| neutral → amber → orange → red | how much of a limit is spent |

**Making the whole application "Claude orange" would be worse than useless.**
The usage ramp would lose its meaning — orange would stop meaning 90% — and the
controls would stop matching the desktop they sit on. The brand colour earns its
place in one artefact.

Terracotta rather than blue because in blue the icon sat in a task manager next
to VS Code looking like one more blue developer tool. Monochrome rather than an
orange dial with a contrasting needle, because at 16 px a second hue is noise.
No dark tile behind it either: a tile forces the mark down to about 78% to leave
padding, costing exactly the legibility that matters most at 16 px, and on a dark
panel it merges into the background while fighting the highlight drawn behind an
active task.

The usage steps used to be defined twice, once in the tray renderer and once in
the QML theme, which is how the popup came to show an accent-blue warning where
the tray showed amber for the same number. They live in
[`src/Brand.h`](../src/Brand.h) now.

## Percentages count up, not down

The API reports what has been spent. Inverting to "remaining" would mean the
notification thresholds are crossed downward, which reads unnaturally in a
banner, and the colour ramp would have to run backwards along the arc.

## The application icon is not the tray icon

They were briefly the same generated `QIcon`, and it went badly in two ways.

Mechanically: a window decoration renders a window icon through a different path
than a panel renders a StatusNotifierItem, and the tray-weight strokes came out
hollow in a title bar — two thin arcs instead of a dial. Every size rendered
solid when drawn directly, including the sizes Qt has to scale, so the distortion
was added downstream and no amount of tuning here would have reached it.

By intent: the tray mark is regenerated per poll, depends on usage, and has its
weights measured for 16–24 px against panel colours we cannot know. An
application icon is static, carries no reading, and has to hold up from 16 px in
a title bar to 256 px in a launcher.

## The popup

340 px wide, and not wider. Two numbers, two bars, two reset lines, and when it
was last updated. It is not an analytics dashboard, and is not going to become
one.

The header mark is always terracotta and always a solid dial — the needle moves
with usage, but the arc does not fade when a fetch fails. An identity should not
dim because the network did.

Separators are kept close to the background colour on purpose: any more contrast
and the card starts reading as a table.
