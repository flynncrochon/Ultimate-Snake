"""Grey barrier contracts, with decisions sampled from the original JavaScript."""
import json
from . import snake_test as st

ORACLE = json.loads((st.ROOT / 'game_tests/fixtures/javascript_grey_snake_oracle.json').read_text())


def _world(p):
    return [v * st.TILE for v in p]


def test_original_javascript_grey_decisions():
    for case in ORACLE['cases']:
        st.reset(seed=710)
        st.player(segments=[_world(case['player_head']), _world(case['player_tail'])], direction=st.EAST)
        if 'other_barrier' in case:
            st.spawn_grey_snake([_world(case['other_barrier'])], alive=False)
        path = [_world(case['head'])]
        if 'own_barrier' in case:
            path.insert(0, _world(case['own_barrier']))
        st.spawn_grey_snake(path, target=_world(case['target']), wall=case['wall'],
                            wall_direction=case['wall_direction'], wall_ticks=case['wall_ticks'])
        result = st.step(4, full=True, validate=True)['grey_trails'][-1]
        expected = case['expected']
        assert result['path'][-1] == _world(expected['head']), (case, result)
        assert result['direction'] == expected['direction'], (case, result)
        assert result['wall'] == expected['wall'] and result['wall_ticks'] == expected['wall_ticks'], (case, result)


def test_spawn_schedule_lifetime_permanent_trails_and_pause():
    st.configure(grey_snakes=True, elapsed=89.96)
    st.step(2)
    assert st.snapshot()['counts']['grey_snakes'] == 0
    st.step(1)
    state = st.snapshot()
    assert state['counts']['grey_snakes'] == 1 and state['counts']['grey_barriers'] == 1, state
    first = state['grey_trails'][0]
    head = first['path'][0]
    assert 25 <= (head[0] - 800) / st.TILE <= 35 and head[1] == 800
    assert 15 <= (first['target'][0] - 800) / st.TILE <= 24
    st.step(240)
    before = st.snapshot()
    st.pause()
    st.step(120)
    paused = st.snapshot()
    assert paused['grey_trails'] == before['grey_trails'] and paused['grey_system'] == before['grey_system']
    st.pause()
    st.step(360)
    dead = st.snapshot()
    assert dead['counts']['grey_snakes'] == 0 and dead['counts']['grey_trails'] == 1
    assert len(dead['grey_trails'][0]['path']) == 167, dead['grey_trails']
    assert dead['grey_trails'][0]['age'] == 10
    path = dead['grey_trails'][0]['path']
    st.configure(grey_clock=179.95)
    st.step(4)
    again = st.snapshot()
    assert again['counts']['grey_snakes'] == 1 and again['counts']['grey_trails'] == 1
    assert again['grey_trails'][0]['path'] == path
    first_cells = {tuple(p) for p in path}
    for p in again['grey_trails'][1]['path']:
        assert all(abs(p[0]-q[0]) > st.TILE or abs(p[1]-q[1]) > st.TILE for q in first_cells)
    st.clear()
    assert not st.snapshot()['grey_trails'] and st.snapshot()['counts']['grey_barriers'] == 0


test_spawn_schedule_lifetime_permanent_trails_and_pause.timeout_ticks = 740


def test_barrier_grace_turn_recovery_and_lethal_immunity_bypass():
    st.configure(freeze_player=False)
    st.spawn_grey_snake([[808,800]], alive=False)
    st.equip('serpents_scales', 8)
    st.player(direction=st.EAST, move_clock=.119)
    st.step(1)
    assert st.player()['position'] == [800,800] and st.player()['collision_clock'] == 0
    st.step(50)
    assert st.snapshot()['phase'] == 'playing' and st.player()['hp'] == 100
    st.step(8, st.NORTH)
    assert st.player()['collision_clock'] < 0 and st.player()['position'][1] < 800

    st.reset()
    st.configure(freeze_player=False)
    st.spawn_grey_snake([[808,800]], alive=False)
    st.equip('serpents_scales', 8)
    st.player(direction=st.EAST, move_clock=.119)
    st.step(1)
    st.step(70)
    state = st.snapshot()
    assert state['phase'] == 'dead' and state['player']['hp'] == 0
    assert state['shield']['charges'] == 3, state
    assert 'GREY SNAKE' in state['notice']


def test_barriers_are_excluded_from_weapon_targeting_and_boss_loot():
    st.spawn_grey_snake([[832,800]], alive=False)
    st.spawn_enemy(832,800, kind='boss', hp=1)
    st.equip('venom_nova', 8)
    st.step(30)
    state = st.snapshot()
    chests = [p for p in state['pickups'] if p['kind'] == 'chest']
    assert len(chests) == 1 and chests[0]['position'] != [832,800], state
    assert state['counts']['grey_barriers'] == 1 and state['kills'] == 1
    st.step(180)
    assert st.snapshot()['counts']['grey_barriers'] == 1


def test_grey_fixture_validation_is_atomic_and_reset_isolated():
    before = st.snapshot()
    for path in ([], [[800,800],[808,808]], [[0,800]], [[800,800]]*169):
        try:
            st.spawn_grey_snake(path)
        except st.GameTestError:
            pass
        else:
            raise AssertionError(path)
        assert st.snapshot() == before
    st.spawn_grey_snake([[1040,800]])
    st.reset()
    st.configure(elapsed=100)
    st.step(100)
    assert not st.snapshot()['grey_trails'], 'Isolated fixtures enabled scheduled grey spawning'


def test_thirteen_permanent_trails_fit_and_overflow_is_rejected():
    for i in range(13):
        st.spawn_grey_snake([[600+i*24,600]], alive=False)
    before = st.snapshot()
    assert before['counts']['grey_trails'] == 13 and before['counts']['grey_barriers'] == 13
    try:
        st.spawn_grey_snake([[600,640]], alive=False)
    except st.GameTestError:
        pass
    else:
        raise AssertionError('Grey run capacity was exceeded')
    assert st.snapshot() == before
