using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace TensorPlanner
{
    public sealed class TensorPlannerException : Exception
    {
        public TensorPlannerException(string message) : base(message) { }
    }

    public enum Status
    {
        Ok = 0,
        InvalidArgument = 1,
        LimitExceeded = 2,
        NotFound = 3,
        Unsupported = 4,
        NoSolution = 5
    }

    public sealed class Limits
    {
        public Limits(
            int maxObjects = 64,
            int maxFacts = 256,
            int maxGoals = 32,
            int maxCandidates = 512,
            int maxExpansions = 4096,
            int maxPlanLength = 64)
        {
            MaxObjects = maxObjects;
            MaxFacts = maxFacts;
            MaxGoals = maxGoals;
            MaxCandidates = maxCandidates;
            MaxExpansions = maxExpansions;
            MaxPlanLength = maxPlanLength;
        }

        public int MaxObjects { get; private set; }
        public int MaxFacts { get; private set; }
        public int MaxGoals { get; private set; }
        public int MaxCandidates { get; private set; }
        public int MaxExpansions { get; private set; }
        public int MaxPlanLength { get; private set; }
    }

    public sealed class PlannerType
    {
        internal PlannerType(int id, Type clrType, string name)
        {
            Id = id;
            ClrType = clrType;
            Name = name;
        }

        public int Id { get; private set; }
        public Type ClrType { get; private set; }
        public string Name { get; private set; }
    }

    public sealed class PlannerAction
    {
        internal PlannerAction(int id, string name, IReadOnlyList<string> parameterNames)
        {
            Id = id;
            Name = name;
            ParameterNames = parameterNames;
        }

        public int Id { get; private set; }
        public string Name { get; private set; }
        public IReadOnlyList<string> ParameterNames { get; private set; }
        public bool IsValid { get { return Id >= 0; } }
    }

    public sealed class Predicate
    {
        private readonly Type[] _argumentTypes;

        internal Predicate(int id, string name, Type[] argumentTypes)
        {
            Id = id;
            Name = name;
            _argumentTypes = argumentTypes;
        }

        public int Id { get; private set; }
        public string Name { get; private set; }
        public IReadOnlyList<Type> ArgumentTypes { get { return _argumentTypes; } }

        public Atom Create(params string[] parameterNames)
        {
            ValidateArity(parameterNames.Length);
            ArgumentRef[] args = new ArgumentRef[parameterNames.Length];
            for (int index = 0; index < parameterNames.Length; index++)
            {
                args[index] = ArgumentRef.Parameter(parameterNames[index]);
            }
            return new Atom(Id, Name, _argumentTypes, args);
        }

        public Atom Create(params object[] objects)
        {
            ValidateArity(objects.Length);
            ArgumentRef[] args = new ArgumentRef[objects.Length];
            for (int index = 0; index < objects.Length; index++)
            {
                object value = objects[index];
                if (value == null)
                {
                    throw new TensorPlannerException("null object in predicate: " + Name);
                }
                if (value.GetType() != _argumentTypes[index])
                {
                    throw new TensorPlannerException("object type mismatch for predicate: " + Name);
                }
                args[index] = ArgumentRef.Object(value, value.GetType());
            }
            return new Atom(Id, Name, _argumentTypes, args);
        }

        public Atom Call(params string[] parameterNames)
        {
            return Create(parameterNames);
        }

        public Atom Call(params object[] objects)
        {
            return Create(objects);
        }

        private void ValidateArity(int arity)
        {
            if (arity != _argumentTypes.Length)
            {
                throw new TensorPlannerException("wrong arity for predicate: " + Name);
            }
        }
    }

    public sealed class Atom
    {
        internal Atom(int predicateId, string predicateName, IReadOnlyList<Type> argumentTypes, IReadOnlyList<ArgumentRef> arguments)
        {
            PredicateId = predicateId;
            PredicateName = predicateName;
            ArgumentTypes = argumentTypes;
            Arguments = arguments;
        }

        public int PredicateId { get; private set; }
        public string PredicateName { get; private set; }
        public IReadOnlyList<Type> ArgumentTypes { get; private set; }
        public IReadOnlyList<ArgumentRef> Arguments { get; private set; }
    }

    public sealed class ArgumentRef
    {
        private ArgumentRef(bool isParameter, string parameterName, object value, Type clrType)
        {
            IsParameter = isParameter;
            ParameterName = parameterName;
            Value = value;
            ClrType = clrType;
        }

        public bool IsParameter { get; private set; }
        public string ParameterName { get; private set; }
        public object Value { get; private set; }
        public Type ClrType { get; private set; }

        public static ArgumentRef Parameter(string name)
        {
            return new ArgumentRef(true, name, null, typeof(void));
        }

        public static ArgumentRef Object(object value, Type clrType)
        {
            return new ArgumentRef(false, null, value, clrType);
        }
    }

    public sealed class PlanStep
    {
        private readonly IReadOnlyList<object> _arguments;
        private readonly IReadOnlyList<Type> _argumentTypes;

        internal PlanStep(PlannerAction action, IReadOnlyList<object> arguments, IReadOnlyList<Type> argumentTypes)
        {
            Action = action;
            _arguments = arguments;
            _argumentTypes = argumentTypes;
        }

        public PlannerAction Action { get; private set; }
        public string Name { get { return Action.Name; } }

        public bool Is(PlannerAction action)
        {
            return Action.Id == action.Id;
        }

        public T Arg<T>(string parameterName) where T : class
        {
            int index = ParameterIndex(parameterName);
            if (_argumentTypes[index] != typeof(T))
            {
                throw new TensorPlannerException("plan argument has unexpected type: " + parameterName);
            }
            return (T)_arguments[index];
        }

        public override string ToString()
        {
            return Name + "(" + string.Join(", ", Action.ParameterNames.ToArray()) + ")";
        }

        private int ParameterIndex(string name)
        {
            for (int index = 0; index < Action.ParameterNames.Count; index++)
            {
                if (Action.ParameterNames[index] == name)
                {
                    return index;
                }
            }
            throw new TensorPlannerException("unknown action parameter: " + name);
        }
    }

    public sealed class SolveResult
    {
        internal SolveResult(Status status, bool solved, int expansions, int generated, int scorerCalls, IReadOnlyList<PlanStep> steps)
        {
            Status = status;
            Solved = solved;
            Expansions = expansions;
            Generated = generated;
            ScorerCalls = scorerCalls;
            Steps = new ReadOnlyCollection<PlanStep>(steps.ToArray());
        }

        public Status Status { get; private set; }
        public bool Solved { get; private set; }
        public int Expansions { get; private set; }
        public int Generated { get; private set; }
        public int ScorerCalls { get; private set; }
        public IReadOnlyList<PlanStep> Steps { get; private set; }
    }

    public sealed class StateBuilder
    {
        private readonly Planner _planner;
        private readonly Dictionary<object, int> _objectIds = new Dictionary<object, int>(ReferenceComparer.Instance);
        private readonly List<object> _objects = new List<object>();
        private readonly List<Type> _objectTypes = new List<Type>();
        private readonly List<int> _nativeObjectTypes = new List<int>();
        private readonly List<Atom> _facts = new List<Atom>();
        private readonly List<Atom> _goals = new List<Atom>();

        internal StateBuilder(Planner planner)
        {
            _planner = planner;
        }

        internal IReadOnlyList<object> Objects { get { return _objects; } }
        internal IReadOnlyList<Type> ObjectTypes { get { return _objectTypes; } }
        internal IReadOnlyList<int> NativeObjectTypes { get { return _nativeObjectTypes; } }
        internal IReadOnlyList<Atom> Facts { get { return _facts; } }
        internal IReadOnlyList<Atom> Goals { get { return _goals; } }

        public StateBuilder Object<T>(T value) where T : class
        {
            if (value == null)
            {
                throw new TensorPlannerException("null object");
            }
            RegisterObject(typeof(T), value);
            return this;
        }

        public StateBuilder Fact(Atom atom)
        {
            _facts.Add(atom);
            RegisterAtomObjects(atom);
            return this;
        }

        public StateBuilder Edge(Predicate predicate, object a, object b)
        {
            return Fact(predicate.Create(a, b)).Fact(predicate.Create(b, a));
        }

        public StateBuilder Goal(Atom atom)
        {
            _goals.Add(atom);
            RegisterAtomObjects(atom);
            return this;
        }

        internal int[] ResolveObjectArgs(Atom atom)
        {
            int[] args = new int[atom.Arguments.Count];
            for (int index = 0; index < atom.Arguments.Count; index++)
            {
                ArgumentRef arg = atom.Arguments[index];
                if (arg.IsParameter || arg.Value == null)
                {
                    throw new TensorPlannerException("state atom uses action parameter in predicate: " + atom.PredicateName);
                }
                if (!_objectIds.TryGetValue(arg.Value, out args[index]))
                {
                    throw new TensorPlannerException("unknown object in predicate: " + atom.PredicateName);
                }
            }
            return args;
        }

        private int RegisterObject(Type clrType, object value)
        {
            int existingId;
            if (_objectIds.TryGetValue(value, out existingId))
            {
                return existingId;
            }

            int objectId = _objects.Count;
            _objectIds[value] = objectId;
            _objects.Add(value);
            _objectTypes.Add(clrType);
            _nativeObjectTypes.Add(_planner.TypeIdFor(clrType));
            return objectId;
        }

        private void RegisterAtomObjects(Atom atom)
        {
            foreach (ArgumentRef arg in atom.Arguments)
            {
                if (arg.IsParameter || arg.Value == null)
                {
                    throw new TensorPlannerException("state atom uses action parameter in predicate: " + atom.PredicateName);
                }
                RegisterObject(arg.ClrType, arg.Value);
            }
        }
    }

    public sealed class ActionBuilder
    {
        private const byte AddEffect = 1;
        private const byte DeleteEffect = 2;

        private readonly Planner _planner;
        private readonly string _name;
        private readonly Dictionary<string, int> _slotIds = new Dictionary<string, int>();
        private readonly Dictionary<string, Type> _slotTypes = new Dictionary<string, Type>();
        private readonly List<string> _parameterNames = new List<string>();
        private readonly List<int> _argumentTypes = new List<int>();
        private readonly List<Native.ActionLiteral> _preconditions = new List<Native.ActionLiteral>();
        private readonly List<Native.ActionEffect> _effects = new List<Native.ActionEffect>();

        internal ActionBuilder(Planner planner, string name)
        {
            _planner = planner;
            _name = name;
        }

        public ActionBuilder Param<T>(string name)
        {
            if (_slotIds.ContainsKey(name))
            {
                throw new TensorPlannerException("duplicate action parameter: " + name);
            }
            if (_argumentTypes.Count >= Native.MaxParameters)
            {
                throw new TensorPlannerException("too many action parameters in: " + _name);
            }

            PlannerType type = _planner.Type<T>();
            _slotIds[name] = _argumentTypes.Count;
            _slotTypes[name] = typeof(T);
            _parameterNames.Add(name);
            _argumentTypes.Add(type.Id);
            return this;
        }

        public ActionBuilder Require(Atom atom)
        {
            _preconditions.Add(MakeLiteral(atom, 1));
            return this;
        }

        public ActionBuilder Adds(Atom atom)
        {
            _effects.Add(MakeEffect(atom, AddEffect));
            return this;
        }

        public ActionBuilder Removes(Atom atom)
        {
            _effects.Add(MakeEffect(atom, DeleteEffect));
            return this;
        }

        public PlannerAction Commit()
        {
            return _planner.CommitAction(_name, _parameterNames, _argumentTypes, _preconditions, _effects);
        }

        private Native.ActionLiteral MakeLiteral(Atom atom, sbyte sign)
        {
            if (atom.Arguments.Count > Native.MaxArity)
            {
                throw new TensorPlannerException("precondition arity exceeds TP_MAX_ARITY: " + atom.PredicateName);
            }
            sbyte[] slots = ResolveSlots(atom);
            return new Native.ActionLiteral(atom.PredicateId, sign, (byte)atom.Arguments.Count, slots);
        }

        private Native.ActionEffect MakeEffect(Atom atom, byte op)
        {
            if (atom.Arguments.Count > Native.MaxArity)
            {
                throw new TensorPlannerException("effect arity exceeds TP_MAX_ARITY: " + atom.PredicateName);
            }
            sbyte[] slots = ResolveSlots(atom);
            return new Native.ActionEffect(atom.PredicateId, op, (byte)atom.Arguments.Count, slots);
        }

        private sbyte[] ResolveSlots(Atom atom)
        {
            sbyte[] slots = new sbyte[Native.MaxArity];
            for (int index = 0; index < atom.Arguments.Count; index++)
            {
                ArgumentRef arg = atom.Arguments[index];
                if (!arg.IsParameter || arg.ParameterName == null)
                {
                    throw new TensorPlannerException("action atom uses object reference in predicate: " + atom.PredicateName);
                }
                int slot;
                if (!_slotIds.TryGetValue(arg.ParameterName, out slot))
                {
                    throw new TensorPlannerException("unknown action parameter '" + arg.ParameterName + "' in " + atom.PredicateName);
                }
                Type type;
                if (!_slotTypes.TryGetValue(arg.ParameterName, out type) || type != atom.ArgumentTypes[index])
                {
                    throw new TensorPlannerException("parameter type mismatch for predicate: " + atom.PredicateName);
                }
                slots[index] = (sbyte)slot;
            }
            return slots;
        }
    }

    public sealed class Planner : IDisposable
    {
        private readonly Native.DomainHandle _domain;
        private readonly Dictionary<Type, PlannerType> _types = new Dictionary<Type, PlannerType>();
        private readonly Dictionary<int, PlannerAction> _actions = new Dictionary<int, PlannerAction>();
        private bool _disposed;

        public Planner() : this(new Limits()) { }

        public Planner(Limits limits)
        {
            Native.Limits nativeLimits = Native.Limits.From(limits ?? new Limits());
            _domain = Native.tp_domain_create(ref nativeLimits);
            if (_domain.IsInvalid)
            {
                throw new TensorPlannerException("tp_domain_create failed");
            }
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _domain.Dispose();
                _disposed = true;
            }
        }

        public PlannerType Type<T>()
        {
            return Type<T>(typeof(T).Name);
        }

        public PlannerType Type<T>(string name)
        {
            return TypeFor(typeof(T), name);
        }

        public Predicate Predicate<T1>(string name)
        {
            return PredicateFromTypes(name, typeof(T1));
        }

        public Predicate Predicate<T1, T2>(string name)
        {
            return PredicateFromTypes(name, typeof(T1), typeof(T2));
        }

        public Predicate Predicate<T1, T2, T3>(string name)
        {
            return PredicateFromTypes(name, typeof(T1), typeof(T2), typeof(T3));
        }

        public Predicate Predicate<T1, T2, T3, T4>(string name)
        {
            return PredicateFromTypes(name, typeof(T1), typeof(T2), typeof(T3), typeof(T4));
        }

        public ActionBuilder Action(string name)
        {
            return new ActionBuilder(this, name);
        }

        public StateBuilder State()
        {
            return new StateBuilder(this);
        }

        public SolveResult Solve(StateBuilder state)
        {
            ThrowIfDisposed();
            if (state == null)
            {
                throw new ArgumentNullException("state");
            }

            Native.StateHandle nativeState = Native.tp_state_create(_domain, state.NativeObjectTypes.Count, state.NativeObjectTypes.ToArray());
            if (nativeState.IsInvalid)
            {
                nativeState.Dispose();
                throw new TensorPlannerException("tp_state_create failed");
            }

            Native.SolverHandle solver = null;
            Native.SolveResult raw = new Native.SolveResult();
            try
            {
                foreach (Atom fact in state.Facts)
                {
                    int[] args = state.ResolveObjectArgs(fact);
                    Check(Native.tp_state_add_fact(nativeState, fact.PredicateId, (byte)args.Length, args), "add fact");
                }

                foreach (Atom goal in state.Goals)
                {
                    int[] args = state.ResolveObjectArgs(goal);
                    Check(Native.tp_state_add_goal_fact(nativeState, goal.PredicateId, (byte)args.Length, args), "add goal");
                }

                solver = Native.tp_solver_create(_domain);
                if (solver.IsInvalid)
                {
                    throw new TensorPlannerException("tp_solver_create failed");
                }

                Status status = Native.tp_solver_solve(solver, nativeState, ref raw);
                IReadOnlyList<PlanStep> steps = raw.Solved != 0 ? ReadSteps(raw, state) : new List<PlanStep>();
                return new SolveResult(status, raw.Solved != 0, raw.Expansions, raw.Generated, raw.ScorerCalls, steps);
            }
            finally
            {
                Native.tp_solve_result_dispose(ref raw);
                if (solver != null)
                {
                    solver.Dispose();
                }
                nativeState.Dispose();
            }
        }

        internal int TypeIdFor(Type clrType)
        {
            PlannerType type;
            if (!_types.TryGetValue(clrType, out type))
            {
                throw new TensorPlannerException("unregistered object type");
            }
            return type.Id;
        }

        internal PlannerAction CommitAction(
            string name,
            IReadOnlyList<string> parameterNames,
            IReadOnlyList<int> argumentTypes,
            IReadOnlyList<Native.ActionLiteral> preconditions,
            IReadOnlyList<Native.ActionEffect> effects)
        {
            ThrowIfDisposed();
            if (argumentTypes.Count == 0)
            {
                throw new TensorPlannerException("action needs at least one parameter: " + name);
            }
            if (argumentTypes.Count > Native.MaxParameters)
            {
                throw new TensorPlannerException("action parameter count exceeds TP_MAX_PARAMS: " + name);
            }

            int actionId;
            Check(Native.tp_domain_add_action_schema(
                _domain,
                (byte)argumentTypes.Count,
                argumentTypes.ToArray(),
                preconditions.ToArray(),
                preconditions.Count,
                effects.ToArray(),
                effects.Count,
                IntPtr.Zero,
                0,
                IntPtr.Zero,
                0,
                out actionId), "add action schema");

            PlannerAction action = new PlannerAction(actionId, name, new ReadOnlyCollection<string>(parameterNames.ToArray()));
            _actions[actionId] = action;
            return action;
        }

        private PlannerType TypeFor(Type clrType, string name)
        {
            PlannerType type;
            if (_types.TryGetValue(clrType, out type))
            {
                return type;
            }
            type = new PlannerType(_types.Count, clrType, name);
            _types.Add(clrType, type);
            return type;
        }

        private Predicate PredicateFromTypes(string name, params Type[] clrTypes)
        {
            if (clrTypes.Length > Native.MaxArity)
            {
                throw new TensorPlannerException("predicate arity exceeds TP_MAX_ARITY: " + name);
            }

            int[] argTypes = new int[clrTypes.Length];
            for (int index = 0; index < clrTypes.Length; index++)
            {
                argTypes[index] = TypeFor(clrTypes[index], clrTypes[index].Name).Id;
            }

            Native.PredicateDef native = new Native.PredicateDef((byte)clrTypes.Length, argTypes);
            int predicateId;
            Check(Native.tp_domain_add_predicate(_domain, ref native, out predicateId), "add predicate");
            return new Predicate(predicateId, name, clrTypes);
        }

        private IReadOnlyList<PlanStep> ReadSteps(Native.SolveResult raw, StateBuilder state)
        {
            List<PlanStep> steps = new List<PlanStep>(raw.PlanLength);
            int size = Marshal.SizeOf(typeof(Native.CandidateAction));
            for (int index = 0; index < raw.PlanLength; index++)
            {
                IntPtr pointer = IntPtr.Add(raw.PlanActions, index * size);
                Native.CandidateAction action = (Native.CandidateAction)Marshal.PtrToStructure(pointer, typeof(Native.CandidateAction));
                steps.Add(MakeStep(action, state));
            }
            return steps;
        }

        private PlanStep MakeStep(Native.CandidateAction raw, StateBuilder state)
        {
            PlannerAction action;
            if (!_actions.TryGetValue(raw.SchemaId, out action))
            {
                action = new PlannerAction(raw.SchemaId, "action_" + raw.SchemaId, new string[0]);
            }

            List<object> args = new List<object>(raw.Arity);
            List<Type> argTypes = new List<Type>(raw.Arity);
            for (int index = 0; index < raw.Arity; index++)
            {
                int objectId = raw.GetArg(index);
                if (objectId < 0 || objectId >= state.Objects.Count)
                {
                    throw new TensorPlannerException("plan references unknown object id");
                }
                args.Add(state.Objects[objectId]);
                argTypes.Add(state.ObjectTypes[objectId]);
            }
            return new PlanStep(action, args, argTypes);
        }

        private void ThrowIfDisposed()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException("Planner");
            }
        }

        private static void Check(Status status, string operation)
        {
            if (status != Status.Ok)
            {
                throw new TensorPlannerException(operation + " failed with status " + (int)status);
            }
        }
    }

    internal static class Native
    {
        internal const int MaxArity = 4;
        internal const int MaxParameters = 6;

#if UNITY_IOS && !UNITY_EDITOR
        private const string Dll = "__Internal";
#else
        private const string Dll = "tensor_planner";
#endif

        [StructLayout(LayoutKind.Sequential)]
        internal struct Limits
        {
            public int MaxObjects;
            public int MaxFacts;
            public int MaxGoals;
            public int MaxCandidates;
            public int MaxExpansions;
            public int MaxPlanLength;

            public static Limits From(TensorPlanner.Limits limits)
            {
                return new Limits
                {
                    MaxObjects = limits.MaxObjects,
                    MaxFacts = limits.MaxFacts,
                    MaxGoals = limits.MaxGoals,
                    MaxCandidates = limits.MaxCandidates,
                    MaxExpansions = limits.MaxExpansions,
                    MaxPlanLength = limits.MaxPlanLength
                };
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct PredicateDef
        {
            public byte Arity;
            public int Arg0;
            public int Arg1;
            public int Arg2;
            public int Arg3;

            public PredicateDef(byte arity, int[] args)
            {
                Arity = arity;
                Arg0 = args.Length > 0 ? args[0] : 0;
                Arg1 = args.Length > 1 ? args[1] : 0;
                Arg2 = args.Length > 2 ? args[2] : 0;
                Arg3 = args.Length > 3 ? args[3] : 0;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct ActionLiteral
        {
            public int PredicateId;
            public sbyte Sign;
            public byte Arity;
            public sbyte Slot0;
            public sbyte Slot1;
            public sbyte Slot2;
            public sbyte Slot3;

            public ActionLiteral(int predicateId, sbyte sign, byte arity, sbyte[] slots)
            {
                PredicateId = predicateId;
                Sign = sign;
                Arity = arity;
                Slot0 = slots.Length > 0 ? slots[0] : (sbyte)0;
                Slot1 = slots.Length > 1 ? slots[1] : (sbyte)0;
                Slot2 = slots.Length > 2 ? slots[2] : (sbyte)0;
                Slot3 = slots.Length > 3 ? slots[3] : (sbyte)0;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct ActionEffect
        {
            public int PredicateId;
            public byte Op;
            public byte Arity;
            public sbyte Slot0;
            public sbyte Slot1;
            public sbyte Slot2;
            public sbyte Slot3;

            public ActionEffect(int predicateId, byte op, byte arity, sbyte[] slots)
            {
                PredicateId = predicateId;
                Op = op;
                Arity = arity;
                Slot0 = slots.Length > 0 ? slots[0] : (sbyte)0;
                Slot1 = slots.Length > 1 ? slots[1] : (sbyte)0;
                Slot2 = slots.Length > 2 ? slots[2] : (sbyte)0;
                Slot3 = slots.Length > 3 ? slots[3] : (sbyte)0;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct CandidateAction
        {
            public int SchemaId;
            public byte Arity;
            public int Arg0;
            public int Arg1;
            public int Arg2;
            public int Arg3;
            public int Arg4;
            public int Arg5;

            public int GetArg(int index)
            {
                switch (index)
                {
                    case 0: return Arg0;
                    case 1: return Arg1;
                    case 2: return Arg2;
                    case 3: return Arg3;
                    case 4: return Arg4;
                    case 5: return Arg5;
                    default: throw new ArgumentOutOfRangeException("index");
                }
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct SolveResult
        {
            public byte Solved;
            public int Expansions;
            public int Generated;
            public int CandidateGenerationCalls;
            public int IndexRebuilds;
            public int ScorerCalls;
            public int PlanLength;
            public int RemainingGoalCount;
            public long CandidateGenerationTimeUs;
            public long ScorerExportTimeUs;
            public long ScorerTimeUs;
            public long ScorerSortTimeUs;
            public IntPtr PlanActions;
        }

        internal sealed class DomainHandle : SafeHandleZeroOrMinusOneIsInvalid
        {
            private DomainHandle() : base(true) { }

            protected override bool ReleaseHandle()
            {
                tp_domain_destroy(handle);
                return true;
            }
        }

        internal sealed class StateHandle : SafeHandleZeroOrMinusOneIsInvalid
        {
            private StateHandle() : base(true) { }

            protected override bool ReleaseHandle()
            {
                tp_state_destroy(handle);
                return true;
            }
        }

        internal sealed class SolverHandle : SafeHandleZeroOrMinusOneIsInvalid
        {
            private SolverHandle() : base(true) { }

            protected override bool ReleaseHandle()
            {
                tp_solver_destroy(handle);
                return true;
            }
        }

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern DomainHandle tp_domain_create(ref Limits limits);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        private static extern void tp_domain_destroy(IntPtr domain);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Status tp_domain_add_predicate(DomainHandle domain, ref PredicateDef predicateDef, out int predicateIdOut);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Status tp_domain_add_action_schema(
            DomainHandle domain,
            byte arity,
            int[] argTypes,
            ActionLiteral[] preconditions,
            int preconditionCount,
            ActionEffect[] effects,
            int effectCount,
            IntPtr numericPreconditions,
            int numericPreconditionCount,
            IntPtr numericEffects,
            int numericEffectCount,
            out int actionIdOut);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern StateHandle tp_state_create(DomainHandle domain, int objectCount, int[] objectTypes);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        private static extern void tp_state_destroy(IntPtr state);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Status tp_state_add_fact(StateHandle state, int predicateId, byte arity, int[] args);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Status tp_state_add_goal_fact(StateHandle state, int predicateId, byte arity, int[] args);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern SolverHandle tp_solver_create(DomainHandle domain);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        private static extern void tp_solver_destroy(IntPtr solver);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Status tp_solver_solve(SolverHandle solver, StateHandle initialState, ref SolveResult result);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void tp_solve_result_dispose(ref SolveResult result);
    }

    internal sealed class ReferenceComparer : IEqualityComparer<object>
    {
        public static readonly ReferenceComparer Instance = new ReferenceComparer();

        public new bool Equals(object x, object y)
        {
            return object.ReferenceEquals(x, y);
        }

        public int GetHashCode(object obj)
        {
            return RuntimeHelpers.GetHashCode(obj);
        }
    }
}
