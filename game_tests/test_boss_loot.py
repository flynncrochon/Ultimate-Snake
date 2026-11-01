"""Real boss death, visible loot, collection, and durable reward contracts."""
from . import snake_test as st

TIER = "logic"


def test_distant_arena_edge_boss_loot_remains_visible_then_awards_a_rank():
    st.reset(seed=0xC8E57)
    st.player(position=[1576,800], next_xp=1000000)
    st.equip("venom_nova",1)
    st.spawn_enemy(1592,800,hp=1,speed=0,kind="boss")
    state=st.step(1,full=True)
    assert state["bosses_killed"]==1, state
    assert state["chests"]==0 and not state["chest_rewards"], state
    boxes=[pickup for pickup in state["pickups"] if pickup["kind"]=="chest"]
    assert len(boxes)==1, state
    position=boxes[0]["position"]
    assert all(8<=coordinate<=1592 and coordinate%8==0 for coordinate in position), boxes
    assert position not in state["player"]["segments"], state
    st.player(position=position)
    opened=st.step(1,full=True)
    reward=opened["chest_rewards"][0]
    assert opened["chests"]==1 and reward["kind"]=="upgrade", opened
    assert reward["id"]=="venom_nova" and reward["rank"]==2 and reward["title"] and reward["description"], reward
    st.step(20)
    assert st.snapshot()["chest_rewards"]==opened["chest_rewards"]
    assert st.snapshot()["chests"]==1


def test_boss_chest_evolves_the_maxed_weapon_and_records_the_actual_reward():
    st.reset(seed=0xE701)
    st.player(next_xp=1000000)
    st.equip("venom_shot",8)
    st.equip("serpents_scales",1)
    x,y=st.player()["position"]
    st.spawn_enemy(x+32,y,hp=1,speed=0,kind="boss")
    st.wait_until(lambda: st.snapshot(full=False)["bosses_killed"]==1,ticks=60,message="boss defeated by the real gun")
    state=st.snapshot()
    box=next(pickup for pickup in state["pickups"] if pickup["kind"]=="chest")
    st.player(position=box["position"])
    opened=st.step(1,full=True)
    reward=opened["chest_rewards"][0]
    assert opened["chests"]==1 and reward["kind"]=="evolution" and reward["title"]=="Ouroboros", opened
    assert next(item for item in opened["owned"] if item["id"]=="venom_shot")["evolved"]
    assert opened["events"]["evolve"]==1 and opened["events"]["chest"]==1, opened
    st.step(2)
    assert st.snapshot()["chests"]==1


def test_paused_and_merged_chests_keep_every_reward_and_reset_clears_reveals():
    st.reset(seed=0xC003)
    st.player(hp=10)
    st.spawn_pickup(kind="chest",value=3)
    st.pause()
    paused=st.step(10,full=True)
    assert paused["chests"]==0 and len(paused["pickups"])==1 and not paused["chest_rewards"], paused
    st.pause()
    state=st.step(1,full=True)
    assert state["chests"]==3 and state["gems"]==75 and state["player"]["hp"]==85, state
    assert [reward["serial"] for reward in state["chest_rewards"]]==[1,2,3], state
    assert all(reward["kind"]=="treasure" and reward["gold"]==25 and reward["healing"]==25 for reward in state["chest_rewards"]), state
    st.step(10)
    assert st.snapshot()["chests"]==3
    st.reset()
    assert not st.snapshot()["chest_rewards"] and st.snapshot()["chests"]==0


def test_bad_pickup_fixtures_fail_without_mutating_the_world():
    st.reset()
    for arguments in ({"kind":"missing"},{"kind":"chest","value":0},
                      {"kind":"chest","value":101},{"kind":"chest","value":True},
                      {"kind":"chest","position":[800]}):
        try:
            st.command("spawn_pickup",**arguments)
        except st.GameTestError:
            pass
        else:
            raise AssertionError(arguments)
        assert st.snapshot()["counts"]["pickups"]==0
    result=st.spawn_pickup(kind="chest",value=1)
    assert result["position"]==st.player()["position"]
    assert st.step(1)["chests"]==1


for _name,_function in list(globals().items()):
    if _name.startswith("test_") and callable(_function):
        _function.timeout_ticks=300
