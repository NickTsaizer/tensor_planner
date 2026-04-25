using TensorPlanner;

Character player = new Character("player");
Location home = new Location("home");
Location forest = new Location("forest");
Location cave = new Location("cave");

using (Planner planner = new Planner(new Limits(8, 16, 4, 32, 128, 8)))
{
    Predicate at = planner.Predicate<Character, Location>("at");
    Predicate connected = planner.Predicate<Location, Location>("connected");

    PlannerAction move = planner.Action("move")
        .Param<Character>("who")
        .Param<Location>("from")
        .Param<Location>("to")
        .Require(at.Create("who", "from"))
        .Require(connected.Create("from", "to"))
        .Removes(at.Create("who", "from"))
        .Adds(at.Create("who", "to"))
        .Commit();

    StateBuilder state = planner.State()
        .Object(player)
        .Object(home)
        .Object(forest)
        .Object(cave)
        .Fact(at.Create(player, home))
        .Edge(connected, forest, home)
        .Edge(connected, forest, cave)
        .Goal(at.Create(player, cave));

    SolveResult result = planner.Solve(state);
    Assert(result.Status == Status.Ok, "solve status");
    Assert(result.Solved, "solved");
    Assert(result.Steps.Count == 2, "two steps");

    PlanStep step = result.Steps[0];
    Assert(step.Is(move), "move action");
    Assert(step.Name == "move", "move name");
    Assert(object.ReferenceEquals(step.Arg<Character>("who"), player), "who arg");
    Assert(object.ReferenceEquals(step.Arg<Location>("from"), home), "from arg");
    Assert(object.ReferenceEquals(step.Arg<Location>("to"), forest), "to arg");

    PlanStep reverseStep = result.Steps[1];
    Assert(reverseStep.Is(move), "reverse move action");
    Assert(object.ReferenceEquals(reverseStep.Arg<Character>("who"), player), "reverse who arg");
    Assert(object.ReferenceEquals(reverseStep.Arg<Location>("from"), forest), "reverse from arg");
    Assert(object.ReferenceEquals(reverseStep.Arg<Location>("to"), cave), "reverse to arg");

    SolveResult asyncResult = await planner.SolveAsync(state);
    Assert(asyncResult.Status == Status.Ok, "async solve status");
    Assert(asyncResult.Solved, "async solved");
    Assert(asyncResult.Steps.Count == 2, "async two steps");

    using CancellationTokenSource canceledSource = new CancellationTokenSource();
    canceledSource.Cancel();
    bool canceled = false;
    try
    {
        await planner.SolveAsync(state, canceledSource.Token);
    }
    catch (OperationCanceledException)
    {
        canceled = true;
    }
    Assert(canceled, "pre-canceled async solve");
}

void Assert(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException($"Assertion failed: {message}");
    }
}

sealed class Character
{
    public Character(string name) { Name = name; }
    public string Name { get; }
}

sealed class Location
{
    public Location(string name) { Name = name; }
    public string Name { get; }
}
