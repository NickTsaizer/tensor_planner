using TensorPlanner;

Character player = new Character("player");
Location home = new Location("home");
Location forest = new Location("forest");

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
        .Fact(at.Create(player, home))
        .Fact(connected.Create(home, forest))
        .Goal(at.Create(player, forest));

    SolveResult result = planner.Solve(state);
    Assert(result.Status == Status.Ok, "solve status");
    Assert(result.Solved, "solved");
    Assert(result.Steps.Count == 1, "one step");

    PlanStep step = result.Steps[0];
    Assert(step.Is(move), "move action");
    Assert(step.Name == "move", "move name");
    Assert(object.ReferenceEquals(step.Arg<Character>("who"), player), "who arg");
    Assert(object.ReferenceEquals(step.Arg<Location>("from"), home), "from arg");
    Assert(object.ReferenceEquals(step.Arg<Location>("to"), forest), "to arg");
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
