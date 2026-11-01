"""Gridlock is charged by actual turns and suspends pursuers without shielding."""
from . import snake_test as st

TIER = "logic"


def _charge(rank=8):
    st.reset(seed=0x610C)
    st.equip("gridlock",rank=rank)
    st.player(next_xp=1000000)
    st.configure(freeze_player=False)
    for direction in (st.SOUTH,st.EAST,st.NORTH,st.WEST):
        st.step(18,direction=direction,validate=True)
    st.configure(freeze_player=True)
    state = st.snapshot()
    assert state["graviton"]["turns"] == 4 and state["graviton"]["serial"] == 0, state
    return state


def test_four_successful_turns_bank_a_charge_and_ranks_scale_the_lock():
    for rank in range(1,9):
        charged = _charge(rank)
        x,y = charged["player"]["position"]
        enemy = st.spawn_enemy(x+16,y,hp=100000,speed=64,length=0)
        state = st.step(1,full=True,validate=True)
        lock = state["graviton"]
        assert lock["turns"] == 0 and lock["serial"] == 1 and lock["center"] == [x,y], state
        assert lock["radius"] == (3+(rank-1)//2)*8 and abs(lock["duration"]-(.65+.1*rank)) < .00001, lock
        assert st.enemy(enemy)["gravity_hold"] > 0, state
        assert state["stats"]["armor"] == 0 and state["stats"]["pickup"] == st.TILE, state
        assert not lock["empowered"] and not state["zones"], state


def test_straights_reversals_and_blocked_turns_cannot_charge_gridlock():
    st.reset(seed=0x610D)
    st.equip("gridlock")
    st.configure(freeze_player=False)
    st.step(30,direction=st.EAST)
    assert st.step(20,direction=st.WEST)["graviton"]["turns"] == 0
    st.player(position=[1592,8],direction=st.EAST,move_clock=.119)
    state = st.step(10,direction=st.NORTH)
    assert state["player"]["position"] == [1592,8] and state["graviton"]["turns"] == 0, state


def test_lock_pins_only_cast_targets_then_resumes_from_their_movement_phase():
    charged = _charge()
    x,y = charged["player"]["position"]
    target = st.spawn_enemy(x+32,y,hp=100000,speed=96,length=0)
    outside = st.spawn_enemy(x+56,y,hp=100000,speed=96,length=0)
    cast = st.step(1,full=True)
    before = st.enemy(target)
    held = st.step(30,full=True,validate=True)
    pinned = st.enemy(target)
    assert pinned["position"] == before["position"] and pinned["move_clock"] == before["move_clock"], held
    assert pinned["gravity_hold"] > 0 and st.enemy(outside)["gravity_hold"] == 0, held
    assert held["graviton"]["center"] == cast["graviton"]["center"], held
    st.step(65,validate=True)
    released = st.enemy(target)
    assert released["gravity_hold"] == 0 and released["position"] != before["position"], released


def test_pinned_enemies_can_be_killed_and_still_deal_normal_contact_damage():
    charged = _charge()
    x,y = charged["player"]["position"]
    st.configure(invulnerable=False)
    enemy = st.spawn_enemy(x,y,hp=1,speed=64,length=0)
    contact = st.step(1,full=True)
    assert contact["player"]["hp"] == 80 and st.enemy(enemy)["gravity_hold"] > 0, contact
    st.equip("venom_nova",rank=8)
    killed = st.step(1,full=True)
    assert killed["kills"] == 1 and st.enemy(enemy) is None, killed
    assert all(all(coordinate%8 == 0 for coordinate in pickup["position"]) for pickup in killed["pickups"]), killed


def test_pause_unequip_clear_and_reset_keep_no_orphaned_gravity_state():
    charged = _charge()
    x,y = charged["player"]["position"]
    enemy = st.spawn_enemy(x+32,y,hp=100000,speed=64,length=0)
    before = st.step(1,full=True)
    st.pause()
    paused = st.step(30,full=True)
    assert paused["graviton"] == before["graviton"] and st.enemy(enemy)["gravity_hold"] == before["enemies"][0]["gravity_hold"], paused
    st.pause()
    st.unequip("gridlock")
    released = st.step(1,full=True)
    assert released["graviton"]["rank"] == 0 and st.enemy(enemy)["gravity_hold"] == 0, released
    st.clear()
    assert st.snapshot()["graviton"]["serial"] == 0
    st.reset()
    assert st.snapshot()["graviton"]["turns"] == 0


def test_gridlock_still_supplies_the_singularity_mortar_recipe():
    st.reset(seed=0x610E)
    st.equip("gridlock",rank=1)
    st.equip("plague_mortar",rank=8,cooldown=1000)
    st.spawn_pickup(kind="chest")
    state = st.step(1,full=True)
    assert state["chest_rewards"][0]["title"] == "Singularity Mortar", state
    assert state["stats"]["pickup"] == 8 and state["stats"]["armor"] == 0, state


def test_singularity_retains_the_turn_trap_and_adds_its_pull_ticks_and_release_blast():
    charged = _charge()
    x,y = charged["player"]["position"]
    # Isolate the inherited trap from independently fired Mortar bombs.
    st.equip("plague_mortar",rank=8,evolved=True,cooldown=1000)
    enemy = st.spawn_enemy(x+24,y,hp=100000,speed=0,length=0)
    cast = st.step(1,full=True,validate=True)
    lock = cast["graviton"]
    wells = [zone for zone in cast["zones"] if zone["gridlock"]]
    assert lock["empowered"] and lock["serial"] == 1 and lock["turns"] == 0, cast
    assert lock["center"] == [x,y] and len(wells) == 1, cast
    assert wells[0]["position"] == lock["center"] and wells[0]["radius"] == lock["radius"], wells
    assert st.enemy(enemy)["gravity_hold"] > 0 and st.enemy(enemy)["hp"] == 99655, cast
    st.step(10,validate=True)
    pulled = st.enemy(enemy)
    assert pulled["position"][0] < x+24 and pulled["gravity_hold"] > 0, pulled
    st.step(80,validate=True)
    released = st.snapshot()
    assert not any(zone["gridlock"] for zone in released["zones"]), released
    # Cobra's 690-damage evolved Mortar: four half-strength 345 ticks,
    # followed by one half-strength 2156 detonation.
    assert st.enemy(enemy)["gravity_hold"] == 0 and st.enemy(enemy)["hp"] == 96464, released


def test_singularity_still_fires_normal_bombs_alongside_the_inherited_trap():
    charged = _charge()
    x,y = charged["player"]["position"]
    st.equip("plague_mortar",rank=8,evolved=True)
    st.spawn_enemy(x+32,y,hp=1000000,speed=0,length=0)
    cast = st.step(1,full=True)
    assert cast["graviton"]["empowered"] and any(zone["gridlock"] for zone in cast["zones"]), cast
    assert any(shot["pattern"] == "mortar" and shot["evolved"] for shot in cast["projectiles"]), cast
    landed = st.step(65,full=True)
    assert any(zone["pattern"] == "mortar" and zone["evolved"] and not zone["gridlock"] for zone in landed["zones"]), landed


for _name,_function in list(globals().items()):
    if _name.startswith("test_") and callable(_function):
        _function.timeout_ticks = 800
