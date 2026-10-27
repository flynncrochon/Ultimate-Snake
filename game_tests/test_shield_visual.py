"""Check continuous shield highlights against an independent body pixel mask."""
from . import snake_test as st
from . import _png
from .test_turns_visual import _frame, _oracle, _rotate, _f32

TIER = "visual"
OUTPUT = st.ROOT / "artifacts/gametest/shield"
WIDTH, HEIGHT = 800, 500
BASE, TOP, LEFT = (37, 85, 126), (141, 232, 248), (67, 172, 209)
SHIELD_COLORS = {bytes(BASE), bytes(TOP), bytes(LEFT)}


def _native():
    if not st.current().native:
        st.skip("Shield pixel checks require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)


def _runs(points):
    run = []
    for point in sorted(points):
        if run and point != run[-1] + 1:
            yield run
            run = []
        run.append(point)
    if run:
        yield run


def _check(frame, alpha, label):
    st.reset(seed=713)
    st.configure(invulnerable=False)
    fields = dict(segments=frame["current"], previous_segments=frame["previous"],
                  direction=list(frame["direction"]), move_clock=alpha * .12,
                  move_interval=.12, invulnerable=0)
    st.player(**fields)
    plain_path = OUTPUT / "plain.png"
    st.capture(plain_path, world_only=True)
    plain = _png.read(plain_path)
    st.equip("serpents_scales", rank=8)
    st.step(1)
    player = st.player(**fields)
    displayed_alpha = _f32(player["move_clock"] / player["move_interval"])
    body, _, _ = _oracle(frame, displayed_alpha)
    # The 18px shield is a two-pixel square dilation of the 14px body.
    outer = {(y + dy) * WIDTH + x + dx
             for pixel in body for x, y in [(pixel % WIDTH, pixel // WIDTH)]
             for dy in range(-2, 3) for dx in range(-2, 3)
             if 0 <= x + dx < WIDTH and 0 <= y + dy < HEIGHT}
    before = st.snapshot()
    before.pop("timing", None)
    path = OUTPUT / f"{label}.png"
    st.capture(path, world_only=True)
    image = _png.read(path)
    after = st.snapshot()
    after.pop("timing", None)
    assert after == before, "Rendering changed shield or simulation state"
    assert (image.width, image.height) == (WIDTH, HEIGHT)
    actual = {pixel for pixel in range(WIDTH * HEIGHT)
              if image.rgba[pixel * 4:pixel * 4 + 3] in SHIELD_COLORS}
    assert actual == outer - body, (label, "Shield detached from the body")
    changed = {pixel for pixel in range(WIDTH * HEIGHT)
               if image.rgba[pixel * 4:pixel * 4 + 4] != plain.rgba[pixel * 4:pixel * 4 + 4]}
    assert changed == outer - body, (label, "Shield changed body or background pixels")
    # Every long exposed north/west edge must be lit continuously. Exclude
    # the two corner pixels where the bevel changes direction.
    north, west = {}, {}
    for pixel in outer:
        x, y = pixel % WIDTH, pixel // WIDTH
        if pixel - WIDTH not in outer:
            north.setdefault(y, []).append(x)
        if x == 0 or pixel - 1 not in outer:
            west.setdefault(x, []).append(y)
    for y, columns in north.items():
        for run in _runs(columns):
            if len(run) >= 8:
                for x in run[2:-2]:
                    assert image.pixel(x, y)[:3] == TOP, (label, "Dotted top highlight", x, y)
    for x, rows in west.items():
        for run in _runs(rows):
            if len(run) >= 8:
                for y in run[2:-2]:
                    assert image.pixel(x, y)[:3] == LEFT, (label, "Dotted left highlight", x, y)


def test_shield_highlights_are_continuous_through_straights_and_turns():
    _native()
    frames = {
        "straight": _frame([(800 - i * 8, 800) for i in range(10)], (1, 0)),
        "turn": _frame([(800, 800), (792, 800), (784, 800), (784, 808),
                        (784, 816), (776, 816), (768, 816)], (0, -1)),
        "tight": _frame([(800, 800), (792, 800), (792, 808), (800, 808),
                         (808, 808), (808, 816)], (0, -1)),
    }
    for name, frame in frames.items():
        for rotation in range(4):
            for alpha in (0, .25, .5, .75, 1):
                _check(_rotate(frame, rotation), alpha, f"{name}-r{rotation}-a{alpha}")


def test_maximum_length_shield_has_continuous_highlights():
    _native()
    points = [(800, 800)]
    directions = ((-1, 0), (0, 1), (1, 0), (0, -1))
    leg = 0
    while len(points) < 120:
        dx, dy = directions[leg % 4]
        for _ in range(1 + leg // 2):
            if len(points) == 120:
                break
            points.append((points[-1][0] + dx * 8, points[-1][1] + dy * 8))
        leg += 1
    _check(_frame(points, (0, -1)), .5, "spiral120")


test_shield_highlights_are_continuous_through_straights_and_turns.timeout_ticks = 60
test_maximum_length_shield_has_continuous_highlights.timeout_ticks = 1
