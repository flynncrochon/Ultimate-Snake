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
    # Unsafe steps may hold instead of sealing an escape. Growth still fits
    # the same 60ms cadence and the head retires at exactly ten seconds.
    assert 2 <= len(dead['grey_trails'][0]['path']) <= 167, dead['grey_trails']
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


def _drive(direction, cells):
    for _ in range(cells):
        before = st.player()['position']
        for _ in range(9):
            state = st.step(1, direction)
            assert state['phase'] == 'playing', state
            if state['player']['position'] != before:
                break
        else:
            raise AssertionError(('route obstructed', before, direction, state))


def test_repeated_rectangle_is_cut_across_its_upcoming_corner():
    st.configure(grey_snakes=True, freeze_player=False, grey_next_spawn=1200)
    st.player(position=[800,800], segments=[[800,800]], length=1, move_clock=0)
    for _ in range(2):
        for direction, cells in ((st.EAST,12),(st.SOUTH,10),(st.WEST,12),(st.NORTH,10)):
            _drive(direction, cells)
    before = st.snapshot()
    assert before['grey_system']['predictability'] == 1, before['grey_system']
    st.configure(grey_next_spawn=0)
    st.step(1)
    spawned = st.snapshot()
    grey = spawned['grey_trails'][0]
    assert grey['cutting'] and grey['wall_direction'][1] == 0, grey
    assert grey['target'][1] > 800, 'Grey snake ignored the upcoming turn south'
    first = grey['path'][0]
    assert abs(first[0]-800)+abs(first[1]-800) >= 12*st.TILE, grey
    for _ in range(180):
        p = st.player()['position']
        state = st.step(1, st.EAST if p[1] == 800 and p[0] < 896 else st.SOUTH)
        assert state['phase'] == 'playing', state
        if state['player']['collision_clock'] >= 0:
            break
    else:
        raise AssertionError('Continuing the learned rectangle escaped the interception')
    assert state['player']['position'][0] == 896, state['player']


test_repeated_rectangle_is_cut_across_its_upcoming_corner.timeout_ticks = 850


def test_straight_pressure_spawns_early_and_pause_freezes_its_history():
    st.configure(grey_snakes=True, freeze_player=False)
    st.player(position=[320,800], segments=[[320,800]], length=1, move_clock=0)
    st.step(590, st.EAST)
    before = st.snapshot()
    assert not before['grey_trails'] and before['grey_system']['predictability'] == 1, before['grey_system']
    st.pause()
    st.step(120, st.NORTH)
    assert st.snapshot()['grey_system'] == before['grey_system']
    st.pause()
    st.step(12, st.EAST)
    after = st.snapshot()
    assert len(after['grey_trails']) == 1 and after['grey_trails'][0]['cutting'], after['grey_system']
    assert 39.99 <= after['grey_system']['spawn_time'] <= 40.1, after['grey_system']


test_straight_pressure_spawns_early_and_pause_freezes_its_history.timeout_ticks = 730


def _wall_leg(path, direction, cells):
    for _ in range(cells):
        p = path[-1]
        path.append([p[0]+direction[0]*st.TILE, p[1]+direction[1]*st.TILE])


def test_grey_preserves_the_last_doorway_and_player_can_escape():
    path = [[800,720]]
    for direction, cells in ((st.EAST,10),(st.SOUTH,20),(st.WEST,20),(st.NORTH,20),(st.EAST,8)):
        _wall_leg(path, direction, cells)
    st.spawn_grey_snake(path, wall=True, wall_direction=st.EAST, target=[808,720], age=9.93)
    state = st.step(4, full=True, validate=True)
    assert state['grey_trails'][0]['path'][-1] in ([784,712],[784,728]), state['grey_trails'][0]
    assert [792,720] not in state['grey_trails'][0]['path'], 'Grey snake sealed the final opening'
    st.step(1)
    assert not st.snapshot()['grey_trails'][0]['alive']
    st.configure(freeze_player=False)
    st.player(position=[792,800], segments=[[792,800]], length=1, direction=st.NORTH, move_clock=0)
    _drive(st.NORTH,20)
    assert st.player()['position'] == [792,640], 'Player could not traverse the preserved exit'


def test_grey_cannot_close_a_pocket_against_the_arena_edge():
    path = [[8,720]]
    for direction, cells in ((st.EAST,9),(st.SOUTH,20),(st.WEST,8)):
        _wall_leg(path, direction, cells)
    st.player(position=[40,800], segments=[[40,800]], length=1)
    st.spawn_grey_snake(path, wall=True, wall_direction=st.WEST, target=[8,880], age=9.93)
    state = st.step(4, full=True, validate=True)
    assert [8,880] not in state['grey_trails'][0]['path'], state['grey_trails'][0]
    st.step(1)
    st.configure(freeze_player=False)
    st.player(position=[8,800], segments=[[8,800]], length=1, direction=st.SOUTH, move_clock=0)
    _drive(st.SOUTH,20)
    assert st.player()['position'] == [8,960], 'Arena-edge exit was not usable'


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


def test_adaptive_run_capacity_fits_and_overflow_is_rejected():
    capacity = st.snapshot()['grey_system']['trail_capacity']
    assert capacity == int(st.command('hello')['run_seconds'] / 30)
    for i in range(capacity):
        st.spawn_grey_snake([[400+(i%10)*24,400+(i//10)*24]], alive=False)
    before = st.snapshot()
    assert before['counts']['grey_trails'] == capacity and before['counts']['grey_barriers'] == capacity
    try:
        st.spawn_grey_snake([[600,640]], alive=False)
    except st.GameTestError:
        pass
    else:
        raise AssertionError('Grey run capacity was exceeded')
    assert st.snapshot() == before
