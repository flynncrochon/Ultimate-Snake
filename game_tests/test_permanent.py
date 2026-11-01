"""Permanent stat budgets through the actual native C++ run reset."""
import math
from . import snake_test as st

MAXIMUM = dict(might=5, vitality=3, armor=3, recovery=5, cooldown=2,
               growth=5, reroll=5, banish=5)


def test_inheritance_applies_on_reset_and_plain_reset_clears_it():
    state = st.reset(permanent=MAXIMUM)
    assert state['permanent_upgrades'] == MAXIMUM
    assert math.isclose(state['stats']['damage'], 1.4375)
    assert math.isclose(state['stats']['growth'], 1.15)
    assert math.isclose(state['stats']['cooldown'], .95, abs_tol=1e-6)
    assert state['stats']['armor'] == 3 and state['stats']['recovery'] == .5
    assert math.isclose(state['player']['max_hp'], 133.1, abs_tol=1e-4)
    assert state['player']['hp'] == state['player']['max_hp']
    assert state['stats']['speed'] == state['stats']['area'] == state['stats']['duration'] == 1
    assert state['stats']['amount'] == 0 and state['stats']['pickup'] == 8
    reset = st.reset()
    assert not any(reset['permanent_upgrades'].values())
    assert reset['player']['max_hp'] == 100 and reset['stats']['damage'] == 1.15


def test_three_percent_growth_survives_many_small_pickups():
    st.reset(permanent={'growth': 1})
    st.player(next_xp=1000000)
    for _ in range(100):
        st.spawn_pickup(value=1)
        state = st.step(1)
    assert state['player']['xp'] == 103
    assert abs(state['player']['xp_remainder']) < 1e-8
    assert state['player']['length'] == 107


def test_recovery_freezes_in_pause_and_does_not_revive_death():
    st.reset(permanent={'recovery': 5})
    st.player(hp=50)
    state = st.step(60)
    assert math.isclose(state['player']['hp'], 50.5, abs_tol=.002)
    st.pause()
    assert st.step(120)['player']['hp'] == state['player']['hp']
    st.pause()
    st.configure(invulnerable=False)
    st.player(hp=1)
    st.spawn_enemy(800, 800, hp=1000000)
    dead = st.step(1)
    assert dead['phase'] == 'dead'
    assert st.step(60)['phase'] == 'dead'


def test_bad_permanent_ranks_fail_before_mutating_the_run():
    st.equip('venom_shot', rank=3)
    before = st.snapshot()
    for invalid in ({'might': 6}, {'vitality': -1}, {'recovery': True},
                    {'missing': 1}, True, [1, 2]):
        try:
            st.reset(seed=7, permanent=invalid)
        except st.GameTestError:
            pass
        else:
            raise AssertionError(invalid)
        assert st.snapshot() == before


def test_seeded_inheritance_is_reproducible():
    def run():
        st.reset(seed=4531, permanent=MAXIMUM)
        st.equip('viper_fangs', rank=3)
        st.spawn_enemy(864, 800, hp=1000000, speed=16)
        state = st.step(120, full=True)
        state.pop('timing')
        return state
    assert run() == run()


for _name, _function in list(globals().items()):
    if _name.startswith('test_') and callable(_function):
        _function.timeout_ticks = 300
