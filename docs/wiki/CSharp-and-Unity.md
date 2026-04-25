# C# and Unity

Tensor Planner includes a Unity-first C# wrapper over the native C ABI.

Main source file:

```text
dev.nick.tensor-planner/Runtime/TensorPlanner.cs
```

The .NET project links the same file:

```text
csharp/TensorPlanner/TensorPlanner.csproj
```

## Package identity

Unity package:

```json
{
  "name": "dev.nick.tensor-planner",
  "displayName": "Tensor Planner",
  "version": "0.1.0",
  "unity": "2019.4"
}
```

## Minimal C# example

```csharp
using TensorPlanner;

sealed class Character { public Character(string name) { Name = name; } public string Name { get; } }
sealed class Location { public Location(string name) { Name = name; } public string Name { get; } }

Character player = new Character("player");
Location home = new Location("home");
Location forest = new Location("forest");

using (Planner planner = new Planner()) {
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
    foreach (PlanStep step in result.Steps) {
        if (step.Is(move)) {
            Character who = step.Arg<Character>("who");
            Location to = step.Arg<Location>("to");
        }
    }
}
```

## API shape

Main types:

- `Planner`
- `Limits`
- `Predicate`
- `PlannerAction`
- `ActionBuilder`
- `StateBuilder`
- `SolveResult`
- `PlanStep`

Common methods:

```csharp
Predicate at = planner.Predicate<Character, Location>("at");

PlannerAction move = planner.Action("move")
    .Param<Character>("who")
    .Param<Location>("from")
    .Param<Location>("to")
    .Require(at.Create("who", "from"))
    .Adds(at.Create("who", "to"))
    .Commit();

StateBuilder state = planner.State()
    .Object(player)
    .Fact(at.Create(player, home))
    .Goal(at.Create(player, forest));

SolveResult result = planner.Solve(state);
```

`Predicate.Create(...)` is the preferred factory. `Predicate.Call(...)` remains
as a compatibility alias.

## Unity samples

The package includes two samples.

### Basic Usage

Path:

```text
dev.nick.tensor-planner/Samples~/Basic Usage/BasicUsage.cs
```

It builds the small movement domain:

```text
player starts at home
home is connected to forest
```

Expected plan:

```text
move(player, home, forest)
```

### Tower of Hanoi

Path:

```text
dev.nick.tensor-planner/Samples~/Tower of Hanoi/HanoiTower.cs
```

It models discs and pegs as supports. The planner solves the three-disc puzzle
with one action schema:

```text
move(disc, from, to)
```

The sample logs moves like:

```text
move small disc off medium disc onto right peg
```

## Native library placement

The wrapper uses:

```csharp
DllImport("tensor_planner")
```

For Unity, native plugins should be placed under:

```text
dev.nick.tensor-planner/Runtime/Plugins/x86_64/
```

Linux:

```text
Runtime/Plugins/x86_64/libtensor_planner.so
```

Windows:

```text
Runtime/Plugins/x86_64/tensor_planner.dll
```

The `build.sh` script stages these automatically for selected target OS values.

## Build C# wrapper

```bash
dotnet build csharp/TensorPlanner/TensorPlanner.csproj -c Release
```

Run smoke test:

```bash
dotnet run --project csharp/TensorPlanner.Smoke/TensorPlanner.Smoke.csproj -c Release
```

Build Unity package staging:

```bash
sh ./build.sh -release -target unity -os linux -o ./dist
```

Output:

```text
dist/unity/dev.nick.tensor-planner/
```

## Lifetime rules

- `Planner` implements `IDisposable`; use `using` or call `Dispose()`.
- `StateBuilder` keeps managed references to registered objects.
- Keep the state/result alive while reading object references from plan steps.
- Do not unload or move the native plugin while planner objects exist.
