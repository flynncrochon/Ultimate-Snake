"""Researched run pacing through the shipping JSON-lines Game API."""
from . import snake_test as st


def test_minute_transition_refills_in_bounded_batches_and_pause_freezes_it():
    st.configure(director=True, grey_snakes=False, floods=False,
                 elapsed=659.99, boss_clock=1000000)
    state = st.step(1, full=True)
    assert state['progression']['run_seconds'] == 1800
    assert state['progression']['wave'] == 12
    assert state['counts']['enemies'] == 8
    assert all(enemy['max_hp'] == 180 and enemy['experience'] == 1
               for enemy in state['enemies'])
    st.pause()
    frozen = st.step(120, full=True)
    assert frozen['progression'] == state['progression']
    assert frozen['enemies'] == state['enemies']
    st.pause()
    assert st.step(60)['counts']['enemies'] == 300


def test_transition_growth_expires_without_growing_bonus_segments():
    st.configure(auto_choose=False)
    st.player(level=20, next_xp=795)
    st.spawn_pickup(value=398)
    state = st.step(1)
    assert state['phase'] == 'level_up'
    assert state['player']['level'] == 21 and state['player']['xp'] == 1
    assert state['player']['next_xp'] == 267
    assert state['stats']['growth'] == 1
    assert state['player']['length'] == 120
    assert state['progression']['experience_collected'] == 398


def test_fractional_growth_is_conserved_across_small_fruit_and_resets():
    st.player(next_xp=1000000)
    st.equip('ravenous_maw', rank=1)
    for _ in range(4):
        st.spawn_pickup(value=1)
        state = st.step(1)
    assert state['player']['xp'] == 5
    assert state['player']['length'] == 11
    assert state['player']['xp_remainder'] == 0
    st.reset()
    assert st.player()['xp_remainder'] == 0
    assert st.snapshot()['progression']['experience_collected'] == 0


def test_stage_ends_at_30_minutes_and_does_not_run_a_31st_wave():
    st.configure(elapsed=1799.5)
    state = st.step(30)
    assert state['phase'] == 'victory' and state['elapsed'] == 1800
    assert state['progression']['wave'] == 30
    assert state['progression']['wave_seconds_remaining'] == 0
    assert st.step(60)['elapsed'] == 1800


for _name, _function in list(globals().items()):
    if _name.startswith('test_') and callable(_function):
        _function.timeout_ticks = 300
