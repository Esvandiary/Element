using System.Linq;

namespace Element.AST
{
    // ReSharper disable once ClassNeverInstantiated.Global
    public class DeclaredStruct : DeclaredItem<IntrinsicStruct>, IConstructor<DeclaredStruct>, IScope, IValue, IType
    {
        protected override string Qualifier { get; } = "struct";
        protected override System.Type[] BodyAlternatives { get; } = {typeof(Scope), typeof(Terminal)};
        private bool IsAlias => DeclaredType != null;
        private DeclaredStruct _aliasedType;

        public IScopeItem? this[Identifier id, CompilationContext compilationContext] => Body is Scope scope ? scope[id, compilationContext] : null;

        IScope? IScope.Parent => Parent;
        string IType.Name => Identifier;
        IType IValue.Type => StructType.Instance;

        public bool MatchesConstraint(IValue value, CompilationContext compilationContext) => value switch
        {
            { } v when v.Type != null // null should never compare equal since it signifies lack of identity
                       // A structs type is either:
                       //    Provided by the implementing intrinsic - this allows an intrinsic to control type comparison of it's instances
                       //        For example, literals declared in source need to have the same type as those created using the Num constructor
                       //    This struct declaration - normal structs compare value types using a reference to the struct declarations instance
                       && v.Type == (IsIntrinsic ? (IType)GetImplementingIntrinsic(null) : this) => true,
            _ => false
        };

        public override bool Validate(CompilationContext compilationContext)
        {
            var success = true;
            if (IsAlias)
            {
                if (IsIntrinsic)
                {
                    compilationContext.LogError(20, "Intrinsic struct cannot be an alias");
                    success = false;
                }

                if (PortList != null)
                {
                    compilationContext.LogError(19, $"Struct alias '{ParsedIdentifier}' cannot have ports - remove either the ports or the alias type");
                    success = false;
                }

                var foundConstraint = DeclaredType.ResolveConstraint(Body as IScope ?? Parent, compilationContext);
                if (foundConstraint is DeclaredStruct aliased)
                {
                    _aliasedType = aliased;
                }
                else
                {
                    compilationContext.LogError(20, $"Cannot create alias of non-struct '{foundConstraint}'");
                    success = false;
                }
            }
            else
            {
                success &= ValidateIntrinsic(compilationContext);
                
                if (!IsIntrinsic && DeclaredInputs.Length < 1)
                {
                    compilationContext.LogError(13, $"Non intrinsic '{Location}' must have ports");
                    success = false;
                }
            }

            success &= ValidateScopeBody(compilationContext);


            return success;
        }

        private sealed class StructInstance : ScopeBase, IValue, ISerializable
        {
            IType IValue.Type => DeclaringStruct;
            private DeclaredStruct DeclaringStruct { get; }

            public override IScopeItem? this[Identifier id, CompilationContext compilationContext] =>
                base[id, compilationContext]
                ?? DeclaredFunction.ResolveAsInstanceFunction(id, this, DeclaringStruct, compilationContext);

            public StructInstance(DeclaredStruct declaringStruct, Port[] inputs, IValue[] memberValues)
            {
                DeclaringStruct = declaringStruct;
                _isSerializable = true;
                _members = new ISerializable[memberValues.Length];
                for (var i = 0; i < memberValues.Length; i++)
                {
                    var value = memberValues[i];
                    if (value is ISerializable serializable)
                    {
                        _members[i] = serializable;
                        SerializedSize += serializable.SerializedSize;
                        continue;
                    }

                    _isSerializable = false;
                    break;
                }

                SetRange(inputs.Zip(memberValues, (port, value) => (port.Identifier, value)));
            }

            private readonly bool _isSerializable;
            private readonly ISerializable[] _members;

            public override string Location => $"Instance of {DeclaringStruct}";
            public int SerializedSize { get; }
            public bool Serialize(ref float[] array, ref int position)
            {
                if (!_isSerializable) return false;
                foreach (var m in _members)
                {
                    m.Serialize(ref array, ref position);
                }

                return true;
            }
        }

        public IValue Call(IValue[] arguments, CompilationContext compilationContext) => Call(arguments, this, compilationContext);

        public IValue Call(IValue[] arguments, DeclaredStruct instanceType, CompilationContext compilationContext) =>
            (IsAlias, IsIntrinsic) switch
            {
                (true, _) => _aliasedType.Call(arguments, instanceType, compilationContext),
                (_, true) => GetImplementingIntrinsic(compilationContext)?.Call(arguments, instanceType, compilationContext),
                (_, _) => arguments.ValidateArgumentCount(DeclaredInputs.Length, compilationContext)
                          && arguments.ValidateArgumentConstraints(DeclaredInputs, Body as IScope ?? Parent, compilationContext)
                              ? (IValue)new StructInstance(instanceType, DeclaredInputs, arguments) : CompilationErr.Instance
            };
    }
}