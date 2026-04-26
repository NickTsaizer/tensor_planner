#import "Basic";
#import "Compiler";
#import "File_Utilities";
#import,dir "..";

#run,stallable copy_tensor_planner_runtime_library();

GUIDED_EXPANSION_CAP :: 20000;

Agent :: struct { name: string; }
Item :: struct { name: string; }
Location :: struct { name: string; }

Crafting_Objects :: struct {
    agent:           Agent;
    home:            Location;
    forest:          Location;
    river:           Location;
    cave:            Location;
    deep_cave:       Location;

    flint:           Item;
    fiber:           Item;
    wood:            Item;
    stone:           Item;
    iron_ore:        Item;
    diamond:         Item;
    distractions:    [..] Item;
    flint_axe:       Item;
    stick:           Item;
    stone_pickaxe:   Item;
    iron_pickaxe:    Item;
    diamond_pickaxe: Item;
}

Crafting_Refs :: struct {
    agent:           Object_Ref;
    home:            Object_Ref;
    forest:          Object_Ref;
    river:           Object_Ref;
    cave:            Object_Ref;
    deep_cave:       Object_Ref;

    flint:           Object_Ref;
    fiber:           Object_Ref;
    wood:            Object_Ref;
    stone:           Object_Ref;
    iron_ore:        Object_Ref;
    diamond:         Object_Ref;
    distractions:    [..] Object_Ref;
    flint_axe:       Object_Ref;
    stick:           Object_Ref;
    stone_pickaxe:   Object_Ref;
    iron_pickaxe:    Object_Ref;
    diamond_pickaxe: Object_Ref;
}

Run_Summary :: struct {
    status: TP_Status = .UNSUPPORTED;
    solved:       bool;
    expansions:   s32;
    generated:    s32;
    scorer_calls: s32;
    plan_length:  s32;
    wall_time_us: s64;
}

copy_tensor_planner_runtime_library :: () {
    options := get_build_options();
    output_path := options.output_path;
    if !output_path output_path = #filepath;

    source := tprint("%/../libtensor_planner.so", #filepath);
    dest := tprint("%/libtensor_planner.so", output_path);
    ok := copy_file(source, dest);
    assert(ok);
}

max_s32 :: (a: s32, b: s32) -> s32 {
    if a > b return a;
    return b;
}

crafting_guidance_limits :: (distraction_recipe_count: s32, max_expansions: s32) -> TP_Limits {
    limits := default_limits();
    limits.max_objects     = distraction_recipe_count + 64;
    limits.max_facts       = distraction_recipe_count * 2 + 256;
    limits.max_goals       = 8;
    limits.max_candidates  = max_s32(512, distraction_recipe_count * 16 + 1024);
    limits.max_expansions  = max_expansions;
    limits.max_plan_length = 80;
    return limits;
}

Crafting_Guidance :: #run planner_domain(
    Type.[Agent, Item, Location],
    Predicate.[
        predicate("at", Agent, Location),
        predicate("has", Item),
        predicate("connected", Location, Location),
        predicate("gather_at", Item, Location),
        predicate("home", Location),
        predicate("requires_tool", Item, Item),
        predicate("gather_bare_hands", Item),
        predicate("recipe2_a", Item, Item),
        predicate("recipe2_b", Item, Item),
        predicate("recipe3_a", Item, Item),
        predicate("recipe3_b", Item, Item),
        predicate("recipe3_c", Item, Item),
    ],
    Function.[
        function("count", Item),
    ],
    Action.[
        action(
            "move",
            Param.[param("agent", Agent), param("from", Location), param("to", Location)],
            Atom.[atom("at", "agent", "from"), atom("connected", "from", "to")],
            Atom.[atom("at", "agent", "from")],
            Atom.[atom("at", "agent", "to")]
        ),
        action(
            "gather_free",
            Param.[param("agent", Agent), param("item", Item), param("where", Location)],
            Atom.[
                atom("at", "agent", "where"),
                atom("gather_at", "item", "where"),
                atom("gather_bare_hands", "item"),
            ],
            Numeric_Precondition.[],
            Atom.[],
            Atom.[atom("has", "item")],
            Numeric_Effect.[increase(numeric("count", "item"), 1.0)]
        ),
        action(
            "gather_tool",
            Param.[
                param("agent", Agent),
                param("item", Item),
                param("where", Location),
                param("tool", Item),
            ],
            Atom.[
                atom("at", "agent", "where"),
                atom("gather_at", "item", "where"),
                atom("requires_tool", "item", "tool"),
                atom("has", "tool"),
            ],
            Numeric_Precondition.[],
            Atom.[],
            Atom.[atom("has", "item")],
            Numeric_Effect.[increase(numeric("count", "item"), 1.0)]
        ),
        action(
            "craft2",
            Param.[
                param("agent", Agent),
                param("item", Item),
                param("where", Location),
                param("first", Item),
                param("second", Item),
            ],
            Atom.[
                atom("at", "agent", "where"),
                atom("home", "where"),
                atom("recipe2_a", "item", "first"),
                atom("recipe2_b", "item", "second"),
                atom("has", "first"),
                atom("has", "second"),
            ],
            Numeric_Precondition.[
                numeric("count", "first") >= 1.0,
                numeric("count", "second") >= 1.0,
            ],
            Atom.[],
            Atom.[atom("has", "item")],
            Numeric_Effect.[
                decrease(numeric("count", "first"), 1.0),
                decrease(numeric("count", "second"), 1.0),
                increase(numeric("count", "item"), 1.0),
            ]
        ),
        action(
            "craft3",
            Param.[
                param("agent", Agent),
                param("item", Item),
                param("where", Location),
                param("first", Item),
                param("second", Item),
                param("third", Item),
            ],
            Atom.[
                atom("at", "agent", "where"),
                atom("home", "where"),
                atom("recipe3_a", "item", "first"),
                atom("recipe3_b", "item", "second"),
                atom("recipe3_c", "item", "third"),
                atom("has", "first"),
                atom("has", "second"),
                atom("has", "third"),
            ],
            Numeric_Precondition.[
                numeric("count", "first") >= 1.0,
                numeric("count", "second") >= 1.0,
                numeric("count", "third") >= 1.0,
            ],
            Atom.[],
            Atom.[atom("has", "item")],
            Numeric_Effect.[
                decrease(numeric("count", "first"), 1.0),
                decrease(numeric("count", "second"), 1.0),
                decrease(numeric("count", "third"), 1.0),
                increase(numeric("count", "item"), 1.0),
            ]
        ),
    ],
    #run crafting_guidance_limits(0, GUIDED_EXPANSION_CAP)
);

main :: () {
    log("crafting guidance sequential distraction sweep");
    log("guided_expansion_cap=%", GUIDED_EXPANSION_CAP);

    run_distraction_case(0);
    run_distraction_case(16);
    run_distraction_case(32);
    run_distraction_case(64);
    run_distraction_case(128);
    run_distraction_case(256);
}

run_distraction_case :: (distraction_count: s32) {
    summary := solve_crafting_case(distraction_count, GUIDED_EXPANSION_CAP);
    print_summary(distraction_count, "default_goal_regression", summary);
    assert(summary.status == .OK, "default solve status");
    assert(summary.solved, "default solve solved");
    log("");
}

solve_crafting_case :: (distraction_count: s32, max_expansions: s32) -> Run_Summary {
    domain := Crafting_Guidance;
    domain.limits = crafting_guidance_limits(distraction_count, max_expansions);

    planner := planner_create(domain);
    defer planner_destroy(*planner);

    objects := make_crafting_objects(distraction_count);
    state := build_crafting_state(domain, *objects);

    solve_start := seconds_since_init();
    result := solve(*planner, *state);
    solve_end := seconds_since_init();
    defer solve_result_destroy(*result);

    return .{
        status = .OK,
        solved = result.raw.solved,
        expansions = result.raw.expansions,
        generated = result.raw.generated,
        scorer_calls = result.raw.scorer_calls,
        plan_length = result.raw.plan_length,
        wall_time_us = cast(s64) ((solve_end - solve_start) * 1000000.0),
    };
}

make_crafting_objects :: (distraction_count: s32) -> Crafting_Objects {
    result: Crafting_Objects;
    result.agent     = .{"agent"};
    result.home      = .{"home"};
    result.forest    = .{"forest"};
    result.river     = .{"river"};
    result.cave      = .{"cave"};
    result.deep_cave = .{"deep_cave"};

    result.flint     = .{"flint"};
    result.fiber     = .{"fiber"};
    result.wood      = .{"wood"};
    result.stone     = .{"stone"};
    result.iron_ore  = .{"iron_ore"};
    result.diamond   = .{"diamond"};
    if distraction_count > 0 {
        for 0..distraction_count - 1
            array_add(*result.distractions, Item.{tprint("distraction_recipe_%", it)});
    }
    result.flint_axe       = .{"flint_axe"};
    result.stick           = .{"stick"};
    result.stone_pickaxe   = .{"stone_pickaxe"};
    result.iron_pickaxe    = .{"iron_pickaxe"};
    result.diamond_pickaxe = .{"diamond_pickaxe"};
    return result;
}

build_crafting_state :: (domain: Planner_Domain, objects: *Crafting_Objects) -> State {
    state := state_begin(domain);
    refs := add_crafting_objects(*state, objects);

    fact(*state, "at", refs.agent, refs.home);
    fact(*state, "home", refs.home);
    add_count_values(*state, refs);

    edge(*state, "connected", refs.home, refs.forest);
    edge(*state, "connected", refs.forest, refs.river);
    edge(*state, "connected", refs.forest, refs.cave);
    edge(*state, "connected", refs.cave, refs.deep_cave);

    fact(*state, "gather_at", refs.flint, refs.river);
    fact(*state, "gather_at", refs.fiber, refs.forest);
    fact(*state, "gather_at", refs.wood, refs.forest);
    fact(*state, "gather_at", refs.stone, refs.cave);
    fact(*state, "gather_at", refs.iron_ore, refs.cave);
    fact(*state, "gather_at", refs.diamond, refs.deep_cave);
    fact(*state, "gather_bare_hands", refs.flint);
    fact(*state, "gather_bare_hands", refs.fiber);
    fact(*state, "requires_tool", refs.wood, refs.flint_axe);
    fact(*state, "requires_tool", refs.stone, refs.flint_axe);
    fact(*state, "requires_tool", refs.iron_ore, refs.stone_pickaxe);
    fact(*state, "requires_tool", refs.diamond, refs.iron_pickaxe);

    for refs.distractions {
        fact(*state, "recipe2_a", it, refs.flint);
        fact(*state, "recipe2_b", it, refs.fiber);
    }

    fact(*state, "recipe2_a", refs.flint_axe, refs.flint);
    fact(*state, "recipe2_b", refs.flint_axe, refs.fiber);
    fact(*state, "recipe2_a", refs.stick, refs.wood);
    fact(*state, "recipe2_b", refs.stick, refs.fiber);
    fact(*state, "recipe2_a", refs.stone_pickaxe, refs.stone);
    fact(*state, "recipe2_b", refs.stone_pickaxe, refs.stick);
    fact(*state, "recipe2_a", refs.iron_pickaxe, refs.iron_ore);
    fact(*state, "recipe2_b", refs.iron_pickaxe, refs.stick);
    fact(*state, "recipe3_a", refs.diamond_pickaxe, refs.diamond);
    fact(*state, "recipe3_b", refs.diamond_pickaxe, refs.stick);
    fact(*state, "recipe3_c", refs.diamond_pickaxe, refs.iron_pickaxe);
    goal(*state, "has", refs.diamond_pickaxe);

    return state;
}

add_crafting_objects :: (state: *State, objects: *Crafting_Objects) -> Crafting_Refs {
    refs: Crafting_Refs;
    refs.agent     = object(state, *objects.agent, objects.agent.name);
    refs.home      = object(state, *objects.home, objects.home.name);
    refs.forest    = object(state, *objects.forest, objects.forest.name);
    refs.river     = object(state, *objects.river, objects.river.name);
    refs.cave      = object(state, *objects.cave, objects.cave.name);
    refs.deep_cave = object(state, *objects.deep_cave, objects.deep_cave.name);

    refs.flint     = object(state, *objects.flint, objects.flint.name);
    refs.fiber     = object(state, *objects.fiber, objects.fiber.name);
    refs.wood      = object(state, *objects.wood, objects.wood.name);
    refs.stone     = object(state, *objects.stone, objects.stone.name);
    refs.iron_ore  = object(state, *objects.iron_ore, objects.iron_ore.name);
    refs.diamond   = object(state, *objects.diamond, objects.diamond.name);
    for *objects.distractions {
        ref := object(state, it, it.name);
        array_add(*refs.distractions, ref);
    }
    refs.flint_axe       = object(state, *objects.flint_axe, objects.flint_axe.name);
    refs.stick           = object(state, *objects.stick, objects.stick.name);
    refs.stone_pickaxe   = object(state, *objects.stone_pickaxe, objects.stone_pickaxe.name);
    refs.iron_pickaxe    = object(state, *objects.iron_pickaxe, objects.iron_pickaxe.name);
    refs.diamond_pickaxe = object(state, *objects.diamond_pickaxe, objects.diamond_pickaxe.name);
    return refs;
}

add_count_values :: (state: *State, refs: Crafting_Refs) {
    value(state, "count", 0.0, refs.flint);
    value(state, "count", 0.0, refs.fiber);
    value(state, "count", 0.0, refs.wood);
    value(state, "count", 0.0, refs.stone);
    value(state, "count", 0.0, refs.iron_ore);
    value(state, "count", 0.0, refs.diamond);
    for refs.distractions value(state, "count", 0.0, it);
    value(state, "count", 0.0, refs.flint_axe);
    value(state, "count", 0.0, refs.stick);
    value(state, "count", 0.0, refs.stone_pickaxe);
    value(state, "count", 0.0, refs.iron_pickaxe);
    value(state, "count", 0.0, refs.diamond_pickaxe);
}

print_summary :: (distraction_count: s32, label: string, summary: Run_Summary) {
    solved_text := "no";
    if summary.solved solved_text = "yes";

    log(
        "distractions=% % solved=% status=% expansions=% generated=% scorer_calls=% plan_length=% wall_us=%",
        distraction_count,
        label,
        solved_text,
        status_name(summary.status),
        summary.expansions,
        summary.generated,
        summary.scorer_calls,
        summary.plan_length,
        summary.wall_time_us
    );
}
