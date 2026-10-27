// Execute the pinned, unmodified survivors/grey_snake.js with a deterministic
// clock/random stream. Works in a JS host; no native game formulas are used.
export function collectGreyReference(source) {
    const math = Object.create(Math);
    math.random = () => .25;
    const load = new Function('performance', 'Math',
        source.replace('export class GreySnakeManager', 'class GreySnakeManager') +
        '\nreturn { GreySnakeManager, GREY_TICK_RATE, GREY_LIFETIME, GREY_FIRST_SPAWN, GREY_SPAWN_INTERVAL, WALL_LENGTH };');
    const { GreySnakeManager, ...constants } = load({ now: () => 0 }, math);
    const cases = [
        { name: 'approach', head: [130,100], target: [120,100] },
        { name: 'begin_wall', head: [121,100], target: [120,100] },
        { name: 'sweep', head: [130,100], wall: true, wall_ticks: 5 },
        { name: 'replan_after_twelve', head: [140,100], wall: true, wall_ticks: 11 },
        { name: 'flee_and_abort_wall', head: [104,100], wall: true },
        { name: 'reverse_at_boundary', head: [130,198], wall: true },
        { name: 'perpendicular_at_boundary', head: [198,100], player_head: [195,100], target: [190,105] },
        { name: 'avoid_tail', head: [130,100], target: [120,100], player_tail: [128,100] },
        { name: 'avoid_other_wall', head: [130,100], target: [120,100], other_barrier: [128,100] },
        { name: 'cross_own_trail', head: [130,100], target: [120,100], own_barrier: [129,100] },
        { name: 'blocked_at_both_sides', head: [130,100], target: [120,100], player_tail: [129,100] },
    ];
    for (const c of cases) {
        c.player_head ??= [100,100]; c.player_tail ??= [99,100];
        c.target ??= [120,100]; c.wall_direction = [0,1];
        c.wall ??= false; c.wall_ticks ??= 0;
        const manager = new GreySnakeManager(200);
        const own = new Set([c.head.join(',')]);
        if (c.own_barrier) own.add(c.own_barrier.join(','));
        manager.barriers = new Set(own);
        if (c.other_barrier) manager.barriers.add(c.other_barrier.join(','));
        const gs = { segments: [{x:c.head[0],y:c.head[1]}], direction:{dx:0,dy:1},
            phase:c.wall?'wall':'intercept', target_x:c.target[0],target_y:c.target[1],
            wall_dx:0,wall_dy:1,wall_ticks:c.wall_ticks,own_barriers:own };
        manager._tick(gs, {head:{x:c.player_head[0],y:c.player_head[1]},direction:{dx:1,dy:0},
            segments:[{x:c.player_head[0],y:c.player_head[1]},{x:c.player_tail[0],y:c.player_tail[1]}]});
        c.expected = {head:[gs.segments[0].x,gs.segments[0].y],direction:[gs.direction.dx,gs.direction.dy],
            wall:gs.phase==='wall',wall_ticks:gs.wall_ticks};
    }
    return { reference_commit:'b6cb72e66eebc1291d57efe63ebbe93ec602e863',
        source:'Unmodified original grey_snake.js executed with clock=0 and Math.random=0.25.',
        units:'JavaScript grid cells; multiply by 8 for native world coordinates.', constants, cases };
}
