"""Public API coverage for the real native flood enemy."""
from game_tests import snake_test as st


def test_flood_is_seeded_bounded_and_clear_removes_its_weapon_handle():
    def run():
        st.reset(seed=1942)
        st.spawn_flood(920, 880, count=128, warning=0)
        st.step(30)
        state = st.snapshot()
        assert state['counts']['floods'] == 1 and state['counts']['water_particles'] == 128
        assert state['counts']['enemies'] == 1 and state['enemies'][0]['kind'] == 'flood'
        assert st.invariants()['ok']
        return state['water_particles'], state['flood']
    assert run() == run()
    st.clear()
    state = st.snapshot()
    assert state['counts']['floods'] == state['counts']['water_particles'] == state['counts']['enemies'] == 0
    assert st.invariants()['ok']


def test_invalid_and_over_capacity_flood_fixtures_do_not_mutate_water():
    st.spawn_flood(920, 880, count=48)
    before = st.snapshot()
    for args in ({'count': 1537}, {'warning': -1}, {'direction': [1, 1]}, {'count': 1489}):
        try:
            st.command('spawn_flood', x=920, y=880, **args)
        except st.GameTestError:
            pass
        else:
            raise AssertionError('Malformed/over-capacity flood fixture was accepted')
        assert st.snapshot()['water_particles'] == before['water_particles']
        assert st.snapshot()['counts'] == before['counts']


def test_head_contact_deals_ten_health_after_warning_and_has_a_damage_cooldown():
    st.configure(invulnerable=False)
    st.spawn_flood(800, 800, count=1, direction=(1, 0), warning=0.1)
    st.step(5)
    assert st.snapshot()['phase'] == 'playing' and st.snapshot()['player']['hp'] == 100
    st.step(3)
    state = st.snapshot()
    assert state['phase'] == 'playing' and state['player']['hp'] == 90
    assert state['player']['invulnerable'] > 0
    st.step(30)
    assert st.snapshot()['player']['hp'] == 90


def test_head_contact_respects_fixture_immunity_and_shields():
    st.spawn_flood(800, 800, count=32, warning=0)
    st.step(1)
    assert st.snapshot()['player']['hp'] == 100
    st.configure(invulnerable=False)
    st.equip('serpents_scales', 8)
    st.step(1)
    state = st.snapshot()
    assert state['phase'] == 'playing' and state['player']['hp'] == 100
    assert state['shield']['charges'] == 2


def test_director_flood_schedule_can_be_controlled_independently():
    st.configure(floods=True, flood_clock=59.99, grey_snakes=False)
    st.step(1)
    state = st.snapshot()
    assert state['flood']['serial'] == 1 and state['counts']['water_particles'] == 128
    assert state['flood']['warning'] == 1.5 and not state['flood']['dangerous']
    assert state['counts']['enemies'] == 1


def test_a_normal_run_spawns_the_flood_without_a_fixture_command():
    # Keep the real director, starter weapon, enemies, bosses and progression.
    # Holding the immune head stationary makes the scheduled arrival observable.
    st.reset(seed=1942, isolated=False)
    st.configure(freeze_player=True, invulnerable=True)
    assert st.snapshot()['flood']['enabled']
    st.step(3599)
    assert st.snapshot()['counts']['floods'] == 0
    state = st.step(1, full=True, validate=True)
    assert state['flood']['serial'] == 1 and state['counts']['water_particles'] == 128
    assert not state['flood']['dangerous'] and state['flood']['warning'] == 1.5
    assert any(enemy['kind'] == 'flood' for enemy in state['enemies'])
    assert any(enemy['kind'] != 'flood' for enemy in state['enemies'])
    assert state['owned'], 'The normal run lost its starter/progression equipment'


test_a_normal_run_spawns_the_flood_without_a_fixture_command.timeout_ticks = 3610


def test_minute_arrivals_overlap_and_share_a_target_until_independent_expiry():
    # The connected body wall protects the head through real water collisions.
    body = [[800, y] for y in range(800, 825, 8)]
    body += [[x, 824] for x in range(792, 775, -8)]
    body += [[776, y] for y in range(816, 775, -8)]
    body += [[x, 776] for x in range(784, 825, 8)]
    body += [[824, y] for y in range(784, 825, 8)]
    body += [[816, 824], [808, 824]]
    st.player(segments=body)
    st.configure(floods=True)
    st.step(3599)
    assert st.snapshot()['counts']['floods'] == 0
    first = st.step(1)
    handle = first['flood']['enemy_id']
    assert first['flood']['next_spawn'] == 120 and first['flood']['lifetime'] == 90
    st.step(3599)
    assert st.snapshot()['counts']['water_particles'] == 128
    joined = st.step(1, full=True, validate=True)
    assert joined['flood']['serial'] == joined['flood']['waves'] == 2
    assert joined['flood']['next_spawn'] == 180 and joined['counts']['water_particles'] == 288
    assert joined['flood']['enemy_id'] == handle and joined['counts']['enemies'] == 1
    assert joined['flood']['dangerous'] and joined['water_particles'][0]['age'] > 60
    assert joined['water_particles'][-1]['age'] < 0.02
    younger = st.step(1862, full=True, validate=True)
    assert younger['flood']['waves'] == 1 and younger['counts']['water_particles'] == 160
    assert younger['flood']['enemy_id'] == handle and younger['flood']['dangerous']
    assert all(p['wave'] == 2 for p in younger['water_particles'])
    assert younger['phase'] == 'playing'


test_minute_arrivals_overlap_and_share_a_target_until_independent_expiry.timeout_ticks = 9080


def test_shooting_water_drops_collectible_loot_and_clears_its_target_once():
    st.reset(seed=1942)
    st.spawn_flood(864, 800, count=1, warning=0)
    before = st.snapshot()
    assert before['water_particles'][0]['hp'] == 20
    st.equip('viper_fangs', rank=8)
    st.wait_until(lambda: st.snapshot()['counts']['water_particles'] == 0,
                  ticks=120, message='a real homing shot destroys water')
    state = st.snapshot()
    assert state['kills'] == 1 and state['counts']['enemies'] == 0
    fruit = [drop for drop in state['pickups'] if drop['kind'] == 'experience']
    assert sum(drop['value'] for drop in fruit) == 1
    landing = fruit[0]['position']
    st.step(30)
    state = st.snapshot()
    assert state['kills'] == 1
    assert [drop['position'] for drop in state['pickups']
            if drop['kind'] == 'experience'] == [landing]
    st.player(position=landing, segments=[landing])
    state = st.step(1, full=True, validate=True)
    assert state['player']['xp'] == 1 and state['player']['length'] == 2
    assert not state['pickups'] and state['counts']['snake_deaths'] == 0


def test_enemy_heads_and_trails_do_not_change_water_flow():
    def run(kind=None, head=False):
        st.reset(seed=1942)
        if kind:
            handle = st.spawn_enemy(840 if head else 864, 800, hp=100000,
                                    kind=kind, speed=0 if head else 480)
            if not head:
                # Build a real historical trail through (840, 800), then hold
                # it while water crosses. spawn_enemy does not inject trails.
                st.step(6)
                st.command('set_enemy', id=handle, speed=0)
        st.spawn_flood(840, 824, count=1, direction=(0, -1), warning=0)
        state = st.step(90, full=True, validate=True)
        assert state['phase'] == 'playing' and state['kills'] == 0
        return state['water_particles']
    reference = run()
    for kind in ('red', 'splitter', 'swift', 'boss'):
        for head in (False, True):
            assert run(kind, head) == reference, (kind, head)


test_enemy_heads_and_trails_do_not_change_water_flow.timeout_ticks = 900
